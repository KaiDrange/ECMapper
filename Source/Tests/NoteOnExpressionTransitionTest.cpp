#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/LayoutWrapper.h"
#include "Core/MidiService.h"
#include "Core/OSCMessage.h"
#include "Core/PerformanceEventSink.h"
#include "Core/SettingsWrapper.h"
#include "Core/ZoneWrapper.h"

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

    const juce::String getName() const override { return "NoteOnExpressionTransitionTest"; }
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

bool containsNoteOn(const std::vector<ecm::PerformanceEvent>& events, int noteNumber) {
    for (const auto& event : events) {
        if (event.kind == ecm::PerformanceEventKind::NoteOn && event.noteNumber == noteNumber)
            return true;
    }

    return false;
}

ecm::osc::Message makeKeyMessage(float pressure, float roll, float yaw, uint64_t timestamp) {
    ecm::osc::Message message;
    message.type = ecm::osc::MessageType::Key;
    message.device = ecm::InstrumentType::Alpha;
    message.course = 0;
    message.key = 0;
    message.active = true;
    message.pressure = pressure;
    message.roll = roll;
    message.yaw = yaw;
    message.timestamp = timestamp;
    std::strncpy(message.devId, "test-device", 63);
    return message;
}

void configureMappedKey(ecm::ConfigLookup& configLookup) {
    auto& key = configLookup.keys[0][0];
    key.keyId = { 0, 0, ecm::InstrumentType::Alpha };
    key.mapType = ecm::KeyMappingType::Note;
    key.notes = { 60, -1, -1, -1 };
    key.output = ecm::MidiChannelType::Chan1;
    key.pressure.valueType = ecm::MidiValueType::Off;
    key.roll.valueType = ecm::MidiValueType::CC;
    key.roll.ccNo = 74;
    key.yaw.valueType = ecm::MidiValueType::CC;
    key.yaw.ccNo = 71;
}

bool verifyCenteredNoteOnAndStrideLimitedTransition() {
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
    configureMappedKey(configLookups[0]);

    midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        configLookups,
        midiService.getProtocol(),
        midiService.getVoiceRouter(),
        midiService.getExpressionPolicy()));

    CapturingSink noteOnSink;
    osc::Message outgoingMessage;
    juce::MidiBuffer midiBuffer;

    const float pressures[] = { 0.05f, 0.10f, 0.20f, 0.35f, 0.55f, 0.80f };
    constexpr float measuredRoll = 0.5f;
    constexpr float measuredYaw = -0.6f;
    const uint64_t startTimestamp = 1'000'000;

    for (int i = 0; i < 6; ++i) {
        auto message = makeKeyMessage(pressures[i], measuredRoll, measuredYaw, startTimestamp + static_cast<uint64_t>(i) * 20'000ULL);
        midiService.processMessage(message, outgoingMessage, midiBuffer, noteOnSink, 0, nullptr);
    }

    bool ok = true;
    ok &= expect(noteOnSink.events.size() == 3, "note-on should emit roll, yaw and note-on events");

    const auto* noteOnRoll = findControllerEvent(noteOnSink.events, 74);
    const auto* noteOnYaw = findControllerEvent(noteOnSink.events, 71);
    ok &= expect(noteOnRoll != nullptr, "note-on should emit the configured roll controller");
    ok &= expect(noteOnYaw != nullptr, "note-on should emit the configured yaw controller");
    ok &= expect(containsNoteOn(noteOnSink.events, 60), "note-on should emit the mapped note");

    if (noteOnRoll != nullptr)
        ok &= expectNear(noteOnRoll->value, 0.5f, 1.0e-6f, "roll should start from center on note-on");
    if (noteOnYaw != nullptr)
        ok &= expectNear(noteOnYaw->value, 0.5f, 1.0e-6f, "yaw should start from center on note-on");

    CapturingSink preStrideSink;
    const uint64_t noteOnTimestamp = startTimestamp + 100'000ULL;
    for (int i = 1; i < 64; ++i) {
        auto message = makeKeyMessage(0.80f, measuredRoll, measuredYaw, noteOnTimestamp + static_cast<uint64_t>(i) * 2'000ULL);
        midiService.processMessage(message, outgoingMessage, midiBuffer, preStrideSink, 0, nullptr);
    }

    ok &= expect(preStrideSink.events.empty(), "legacy MIDI mode should not emit extra hold events before the normal stride boundary");

    CapturingSink strideSink;
    auto strideBoundaryMessage = makeKeyMessage(0.80f, measuredRoll, measuredYaw, noteOnTimestamp + 64ULL * 2'000ULL);
    midiService.processMessage(strideBoundaryMessage, outgoingMessage, midiBuffer, strideSink, 0, nullptr);

    const auto* transitionedRoll = findControllerEvent(strideSink.events, 74);
    const auto* transitionedYaw = findControllerEvent(strideSink.events, 71);
    ok &= expect(transitionedRoll != nullptr, "legacy MIDI mode should emit roll on the stride boundary");
    ok &= expect(transitionedYaw != nullptr, "legacy MIDI mode should emit yaw on the stride boundary");

    if (transitionedRoll != nullptr)
        ok &= expect(transitionedRoll->value > 0.5f, "roll should move away from center when the first stride-limited update arrives");
    if (transitionedYaw != nullptr)
        ok &= expect(transitionedYaw->value < 0.5f, "yaw should move away from center when the first stride-limited update arrives");

    midiService.stop();
    return ok;
}

bool verifyMidi2KeyPitchBendScalingMatchesLegacy()
{
    using namespace ecm;

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;

    LayoutWrapper::LayoutKey layoutKey;
    layoutKey.keyId = { 0, 0, InstrumentType::Alpha };
    layoutKey.keyType = EigenharpKeyType::Normal;
    layoutKey.keyColour = KeyColour::Off;
    layoutKey.zone = Zone::Zone1;
    layoutKey.keyMappingType = KeyMappingType::Note;
    layoutKey.mappingValue = "60";
    LayoutWrapper::setLayoutKey(layoutKey, pluginState.state);

    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan1, pluginState.state);
    ZoneWrapper::setKeyPitchbend(InstrumentType::Alpha, Zone::Zone1, 1, pluginState.state);
    ZoneWrapper::setChannelMaxPitchbend(InstrumentType::Alpha, Zone::Zone1, 48, pluginState.state);

    auto getPitchBendRange = [&](bool midi2Mode) {
        SettingsWrapper::setMidi2Mode(midi2Mode, pluginState.state);
        ConfigLookup lookup(InstrumentType::Alpha, pluginState, stateLock);
        lookup.updateAll();
        return lookup.keys[0][0].pbRange;
    };

    const float legacyRange = getPitchBendRange(false);
    const float midi2Range = getPitchBendRange(true);
    const float expectedRange = 1.0f / 48.0f;

    bool ok = true;
    ok &= expectNear(legacyRange, expectedRange, 1.0e-6f, "legacy key pitch bend scaling should match the configured channel max range");
    ok &= expectNear(midi2Range, expectedRange, 1.0e-6f, "MIDI 2.0 key pitch bend scaling should match the configured channel max range");
    return ok;
}

} // namespace

int main() {
    using namespace ecm;

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    const bool ok = verifyCenteredNoteOnAndStrideLimitedTransition()
                 && verifyMidi2KeyPitchBendScalingMatchesLegacy();
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "NoteOnExpressionTransitionTest passed" << std::endl;
    return 0;
}