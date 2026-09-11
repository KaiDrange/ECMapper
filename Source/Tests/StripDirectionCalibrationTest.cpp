#define private public
#include "Core/MidiService.h"
#undef private

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/OSCMessage.h"
#include "Core/PerformanceEventSink.h"
#include "Core/SettingsWrapper.h"
#include "Core/ZoneWrapper.h"

namespace {

constexpr float tolerance = 1.0e-6f;
constexpr int stripController = 20;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool expectNear(float actual, float expected, const char* message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << message << " (expected " << expected << ", got " << actual << ")" << std::endl;
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

    const juce::String getName() const override { return "StripDirectionCalibrationTest"; }
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

class CapturingSink final : public ecm::PerformanceEventSink {
public:
    void pushEvent(const ecm::PerformanceEvent& event) override {
        events.push_back(event);
    }

    std::vector<ecm::PerformanceEvent> events;
};

const ecm::PerformanceEvent* findControllerEvent(const std::vector<ecm::PerformanceEvent>& events, int controllerNumber) {
    for (const auto& event : events) {
        if (event.kind == ecm::PerformanceEventKind::Controller && event.controller == controllerNumber)
            return &event;
    }

    return nullptr;
}

ecm::osc::Message makeStripMessage(float value, bool active, uint64_t timestamp) {
    ecm::osc::Message message;
    message.type = ecm::osc::MessageType::Strip;
    message.device = ecm::InstrumentType::Alpha;
    message.strip = 1;
    message.active = active;
    message.value = value;
    message.timestamp = timestamp;
    std::strncpy(message.devId, "test-device", 63);
    return message;
}

void configureMappedStrip(ecm::ConfigLookup& configLookup, juce::ValueTree& state) {
    ecm::ZoneWrapper::setEnabled(ecm::InstrumentType::Alpha, ecm::Zone::Zone1, true, state);
    ecm::ZoneWrapper::setMidiValue(ecm::InstrumentType::Alpha,
                                   ecm::Zone::Zone1,
                                   ecm::ZoneWrapper::id_strip1Abs,
                                   { ecm::MidiValueType::CC, stripController },
                                   state);
    ecm::ZoneWrapper::setMidiValue(ecm::InstrumentType::Alpha,
                                   ecm::Zone::Zone1,
                                   ecm::ZoneWrapper::id_strip1Rel,
                                   { ecm::MidiValueType::Off, 0 },
                                   state);
    configLookup.updateAll();
}

bool verifyHelperCalibration() {
    using namespace ecm;

    bool ok = true;
    ok &= expectNear(MidiService::applyStripCalibration(0.25f, false, 0.0f, 2.0f), 0.75f,
                     "unchecked invert strip direction should flip the raw EigenLite value");
    ok &= expectNear(MidiService::applyStripCalibration(0.25f, true, 0.0f, 2.0f), 0.25f,
                     "checked invert strip direction should preserve the raw EigenLite value");
    ok &= expectNear(MidiService::applyStripCalibration(0.95f, false, 0.1f, 2.0f), 0.0f,
                     "flipped strip values should still clamp at zero after thresholding");
    return ok;
}

bool verifyLiveCalibrationUpdate() {
    using namespace ecm;

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    configureMappedStrip(configLookups[0], pluginState.state);

    SettingsWrapper::setCalibrationValue(InstrumentType::Alpha, SettingsWrapper::id_stripThreshold, 0.0f, pluginState.state);
    SettingsWrapper::setCalibrationValue(InstrumentType::Alpha, SettingsWrapper::id_stripSensitivity, 2.0f, pluginState.state);
    SettingsWrapper::setCalibrationBool(InstrumentType::Alpha, SettingsWrapper::id_invertStripDirection, false, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        configLookups,
        midiService.getProtocol(),
        midiService.getVoiceRouter(),
        midiService.getExpressionPolicy()));

    osc::Message outgoingMessage;
    juce::MidiBuffer midiBuffer;

    CapturingSink defaultDirectionSink;
    midiService.processMessage(makeStripMessage(0.25f, true, 1'000'000ULL), outgoingMessage, midiBuffer, defaultDirectionSink, 0, nullptr);

    const auto* defaultDirectionEvent = findControllerEvent(defaultDirectionSink.events, stripController);
    bool ok = expect(defaultDirectionEvent != nullptr,
                     "strip processing should emit the configured absolute strip controller");
    if (defaultDirectionEvent != nullptr) {
        ok &= expectNear(defaultDirectionEvent->value, 1.0f,
                         "unchecked invert strip direction should emit the flipped strip value with sensitivity applied once");
    }

    SettingsWrapper::setCalibrationBool(InstrumentType::Alpha, SettingsWrapper::id_invertStripDirection, true, pluginState.state);

    CapturingSink invertedDirectionSink;
    midiService.processMessage(makeStripMessage(0.25f, true, 1'020'000ULL), outgoingMessage, midiBuffer, invertedDirectionSink, 0, nullptr);

    const auto* invertedDirectionEvent = findControllerEvent(invertedDirectionSink.events, stripController);
    ok &= expect(invertedDirectionEvent != nullptr,
                 "strip processing should continue emitting after the invert option changes");
    if (invertedDirectionEvent != nullptr) {
        ok &= expectNear(invertedDirectionEvent->value, 0.5f,
                         "checking invert strip direction should switch back to the legacy strip direction immediately");
    }

    midiService.stop();
    return ok;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    bool ok = verifyHelperCalibration();
    ok &= verifyLiveCalibrationUpdate();

    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "StripDirectionCalibrationTest passed" << std::endl;
    return 0;
}