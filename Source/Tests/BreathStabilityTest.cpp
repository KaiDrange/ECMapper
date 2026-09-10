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

ecm::osc::Message makeBreathMessage(float value, uint64_t timestamp) {
    ecm::osc::Message message;
    message.type = ecm::osc::MessageType::Breath;
    message.device = ecm::InstrumentType::Alpha;
    message.value = value;
    message.timestamp = timestamp;
    std::strncpy(message.devId, "test-device", 63);
    return message;
}

float currentBreathMarker(ecm::MidiService& midiService) {
    const auto markers = midiService.getVisualMarkers(ecm::InstrumentType::Alpha, ecm::ExpressionCurveTarget::Breath);
    if (markers.empty())
        return -1.0f;

    return markers.front().value;
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
    constexpr float breathThreshold = 0.03125f;
    constexpr float breathSensitivity = 1.7f;
    const float expectedMarker = ((inputBreath - breathThreshold) / (1.0f - breathThreshold)) * breathSensitivity;

    midiService.processMessage(makeBreathMessage(inputBreath, 1000), outgoingMessage, midiBuffer, 0, nullptr);
    const float initialMarker = currentBreathMarker(midiService);

    bool ok = true;
    ok &= expect(initialMarker >= 0.0f, "breath marker should be present after a breath message");
    ok &= expectNear(initialMarker, expectedMarker, 1.0e-6f, "breath marker should reflect the normalized breath value");

    for (int i = 0; i < 8; ++i)
        midiService.reduceBreath(midiBuffer, 0, 512);

    const float heldMarker = currentBreathMarker(midiService);
    ok &= expectNear(heldMarker, initialMarker, 1.0e-6f, "steady breath should not sag between nearby updates");

    midiService.processMessage(makeBreathMessage(inputBreath, 2000), outgoingMessage, midiBuffer, 0, nullptr);
    const float refreshedMarker = currentBreathMarker(midiService);
    ok &= expectNear(refreshedMarker, initialMarker, 1.0e-6f, "steady breath should not dip and jump back when the next update arrives");

    for (int i = 0; i < 20; ++i)
        midiService.reduceBreath(midiBuffer, 0, 512);

    const float decayedMarker = currentBreathMarker(midiService);
    ok &= expect(decayedMarker < heldMarker, "breath marker should still decay once the hold period expires");

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
                 && verifyBreathSensitivityDefaultsToOnePointSeven();
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "BreathStabilityTest passed" << std::endl;
    return 0;
}