#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/MidiService.h"
#include "Core/OSCMessage.h"
#include "Core/SettingsWrapper.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool expectNear(float actual, float expected, float tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << message << " expected=" << expected << " actual=" << actual << std::endl;
        return false;
    }

    return true;
}

class SilentLogger : public juce::Logger {
public:
    void logMessage(const juce::String&) override {}
};

class DummyProcessor : public juce::AudioProcessor {
public:
    DummyProcessor()
        : juce::AudioProcessor(BusesProperties()) {
    }

    const juce::String getName() const override { return "BreathStabilityTest"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

ecm::osc::Message makeBreathMessage(float value, uint64_t timestamp, ecm::InstrumentType deviceType) {
    ecm::osc::Message message;
    message.type = ecm::osc::MessageType::Breath;
    message.device = deviceType;
    message.value = value;
    message.timestamp = timestamp;
    std::strncpy(message.devId, "test-device", 63);
    return message;
}

ecm::osc::Message makeBreathMessage(float value, uint64_t timestamp) {
    return makeBreathMessage(value, timestamp, ecm::InstrumentType::Alpha);
}

float currentBreathMarker(ecm::MidiService& midiService, ecm::InstrumentType deviceType) {
    const auto markers = midiService.getVisualMarkers(deviceType, ecm::ExpressionCurveTarget::Breath);
    if (markers.empty())
        return -1.0f;

    return markers.front().value;
}

float expectedBreathMarker(ecm::InstrumentType deviceType, float inputBreath) {
    constexpr float breathThreshold = 0.03125f;
    constexpr float breathSensitivity = 1.7f;
    const float compensatedBreath = (deviceType == ecm::InstrumentType::Alpha || deviceType == ecm::InstrumentType::Tau)
                                  ? std::min(inputBreath * 1.2f, 1.0f)
                                  : inputBreath;
    const float normalizedBreath = compensatedBreath <= breathThreshold
                                  ? 0.0f
                                  : (compensatedBreath - breathThreshold) / (1.0f - breathThreshold);
    return normalizedBreath * breathSensitivity;
}

bool verifySteadyBreathDoesNotSagBetweenNearbyUpdates() {
    using namespace ecm;

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    SettingsWrapper::setMidi2Mode(false, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        configLookups,
        midiService.getProtocol(),
        midiService.getVoiceRouter(),
        midiService.getExpressionPolicy()));

    osc::Message outgoingMessage;
    juce::MidiBuffer midiBuffer;

    constexpr float inputBreath = 0.50f;
    const float expectedMarker = expectedBreathMarker(InstrumentType::Alpha, inputBreath);

    midiService.processMessage(makeBreathMessage(inputBreath, 1000), outgoingMessage, midiBuffer, 0, nullptr);
    const float initialMarker = currentBreathMarker(midiService, InstrumentType::Alpha);

    bool ok = true;
    ok &= expect(initialMarker >= 0.0f, "breath marker should be present after a breath message");
    ok &= expectNear(initialMarker, expectedMarker, 1.0e-6f, "breath marker should reflect the normalized breath value");

    for (int i = 0; i < 8; ++i)
        midiService.reduceBreath(midiBuffer, 0, 512);

    const float heldMarker = currentBreathMarker(midiService, InstrumentType::Alpha);
    ok &= expectNear(heldMarker, initialMarker, 1.0e-6f, "steady breath should not sag between nearby updates");

    midiService.processMessage(makeBreathMessage(inputBreath, 2000), outgoingMessage, midiBuffer, 0, nullptr);
    const float refreshedMarker = currentBreathMarker(midiService, InstrumentType::Alpha);
    ok &= expectNear(refreshedMarker, initialMarker, 1.0e-6f, "steady breath should not dip and jump back when the next update arrives");

    for (int i = 0; i < 20; ++i)
        midiService.reduceBreath(midiBuffer, 0, 512);

    const float decayedMarker = currentBreathMarker(midiService, InstrumentType::Alpha);
    ok &= expect(decayedMarker < heldMarker, "breath marker should still decay once the hold period expires");

    midiService.stop();
    return ok;
}

bool verifyAlphaAndTauBreathAreBoostedBeforeCalibration() {
    using namespace ecm;

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    SettingsWrapper::setMidi2Mode(false, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        configLookups,
        midiService.getProtocol(),
        midiService.getVoiceRouter(),
        midiService.getExpressionPolicy()));

    osc::Message outgoingMessage;
    juce::MidiBuffer midiBuffer;

    constexpr float inputBreath = 0.50f;

    midiService.processMessage(makeBreathMessage(inputBreath, 1000, InstrumentType::Alpha), outgoingMessage, midiBuffer, 0, nullptr);
    midiService.processMessage(makeBreathMessage(inputBreath, 1001, InstrumentType::Tau), outgoingMessage, midiBuffer, 0, nullptr);
    midiService.processMessage(makeBreathMessage(inputBreath, 1002, InstrumentType::Pico), outgoingMessage, midiBuffer, 0, nullptr);

    const float alphaMarker = currentBreathMarker(midiService, InstrumentType::Alpha);
    const float tauMarker = currentBreathMarker(midiService, InstrumentType::Tau);
    const float picoMarker = currentBreathMarker(midiService, InstrumentType::Pico);

    bool ok = true;
    ok &= expect(alphaMarker >= 0.0f, "Alpha breath marker should be present after a breath message");
    ok &= expect(tauMarker >= 0.0f, "Tau breath marker should be present after a breath message");
    ok &= expect(picoMarker >= 0.0f, "Pico breath marker should be present after a breath message");
    ok &= expectNear(alphaMarker, expectedBreathMarker(InstrumentType::Alpha, inputBreath), 1.0e-6f,
                     "Alpha breath should be boosted before threshold calibration is applied");
    ok &= expectNear(tauMarker, expectedBreathMarker(InstrumentType::Tau, inputBreath), 1.0e-6f,
                     "Tau breath should be boosted before threshold calibration is applied");
    ok &= expect(alphaMarker > picoMarker + 0.15f,
                 "Alpha breath should sit materially above Pico for the same raw input after the requested compensation");
    ok &= expect(tauMarker > picoMarker + 0.15f,
                 "Tau breath should sit materially above Pico for the same raw input after the requested compensation");

    midiService.stop();
    return ok;
}

bool verifyBreathSensitivityDefaultsToOnePointSeven() {
    using namespace ecm;

    juce::ValueTree root { "Root" };

    bool ok = true;
    ok &= expectNear(SettingsWrapper::getCalibrationValue(InstrumentType::Alpha, SettingsWrapper::id_breathSensitivity, 1.7f, root), 1.7f, 1.0e-6f,
                     "alpha breath sensitivity should default to 1.7");
    ok &= expectNear(SettingsWrapper::getCalibrationValue(InstrumentType::Tau, SettingsWrapper::id_breathSensitivity, 1.7f, root), 1.7f, 1.0e-6f,
                     "tau breath sensitivity should default to 1.7");
    ok &= expectNear(SettingsWrapper::getCalibrationValue(InstrumentType::Pico, SettingsWrapper::id_breathSensitivity, 1.7f, root), 1.7f, 1.0e-6f,
                     "pico breath sensitivity should default to 1.7");
    return ok;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    const bool ok = verifySteadyBreathDoesNotSagBetweenNearbyUpdates()
                 && verifyAlphaAndTauBreathAreBoostedBeforeCalibration()
                 && verifyBreathSensitivityDefaultsToOnePointSeven();
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "BreathStabilityTest passed" << std::endl;
    return 0;
}