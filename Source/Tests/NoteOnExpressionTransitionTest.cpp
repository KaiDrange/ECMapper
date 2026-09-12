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

ecm::osc::Message makeStripMessage(ecm::InstrumentType deviceType, unsigned int strip, float value, bool active = true)
{
    ecm::osc::Message message;
    message.type = ecm::osc::MessageType::Strip;
    message.device = deviceType;
    message.strip = strip;
    message.active = active;
    message.value = value;
    std::strncpy(message.devId, "test-device", 63);
    return message;
}

ecm::LayoutWrapper::LayoutKey makeLayoutKey(ecm::KeyMappingType mappingType, const juce::String& mappingValue)
{
    ecm::LayoutWrapper::LayoutKey layoutKey;
    layoutKey.keyId = { 0, 0, ecm::InstrumentType::Alpha };
    layoutKey.keyType = ecm::EigenharpKeyType::Normal;
    layoutKey.keyColour = ecm::KeyColour::Off;
    layoutKey.zone = ecm::Zone::Zone1;
    layoutKey.keyMappingType = mappingType;
    layoutKey.mappingValue = mappingValue;
    return layoutKey;
}

bool containsProgramChange(const juce::MidiBuffer& buffer, int channel, int program)
{
    for (const auto metadata : buffer)
    {
        const auto message = metadata.getMessage();
        if (message.isProgramChange() && message.getChannel() == channel && message.getProgramChangeNumber() == program)
            return true;
    }

    return false;
}

bool containsControllerChange(const juce::MidiBuffer& buffer, int channel, int controller, int value)
{
    for (const auto metadata : buffer)
    {
        const auto message = metadata.getMessage();
        if (message.isController() && message.getChannel() == channel
            && message.getControllerNumber() == controller && message.getControllerValue() == value)
            return true;
    }

    return false;
}

int countControllerChangeMessages(const juce::MidiBuffer& buffer, int channel, int controller, int value)
{
    int count = 0;
    for (const auto metadata : buffer)
    {
        const auto message = metadata.getMessage();
        if (message.isController() && message.getChannel() == channel
            && message.getControllerNumber() == controller && message.getControllerValue() == value)
            ++count;
    }

    return count;
}

int countAllNotesOffMessages(const juce::MidiBuffer& buffer)
{
    int count = 0;
    for (const auto metadata : buffer)
        if (metadata.getMessage().isAllNotesOff())
            ++count;

    return count;
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

bool verifyMpeMasterPitchBendUsesChannelMaxRange()
{
    using namespace ecm;

    bool ok = true;
    auto verifyMode = [&](bool midi2Mode) {
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

        SettingsWrapper::setMidi2Mode(midi2Mode, pluginState.state);
        SettingsWrapper::setLowerMPEPB(1, pluginState.state);
        ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::MPE_Low, pluginState.state);
        ZoneWrapper::setKeyPitchbend(InstrumentType::Alpha, Zone::Zone1, 1, pluginState.state);
        ZoneWrapper::setChannelMaxPitchbend(InstrumentType::Alpha, Zone::Zone1, 12, pluginState.state);
        ZoneWrapper::setMidiValue(InstrumentType::Alpha,
                                  Zone::Zone1,
                                  ZoneWrapper::id_breath,
                                  { MidiValueType::Pitchbend, 0 },
                                  pluginState.state);
        ZoneWrapper::setMidiValue(InstrumentType::Alpha,
                                  Zone::Zone1,
                                  ZoneWrapper::id_strip1Abs,
                                  { MidiValueType::Pitchbend, 0 },
                                  pluginState.state);

        ConfigLookup lookup(InstrumentType::Alpha, pluginState, stateLock);
        lookup.updateAll();

        const char* modeName = midi2Mode ? "MIDI 2.0" : "legacy MIDI";
        ok &= expectNear(lookup.keys[0][0].pbRange, 1.0f, 1.0e-6f,
                         midi2Mode
                             ? "MPE member-note pitch bend should use the full range when key pitch bend matches the lower-zone per-note range in MIDI 2.0 mode"
                             : "MPE member-note pitch bend should use the full range when key pitch bend matches the lower-zone per-note range in legacy MIDI mode");
        ok &= expectNear(lookup.breath[0].pbRange, 1.0f, 1.0e-6f,
                         midi2Mode
                             ? "MPE master-channel breath pitch bend should keep the full pitch-bend range in MIDI 2.0 mode"
                             : "MPE master-channel breath pitch bend should keep the full pitch-bend range in legacy MIDI mode");
        ok &= expectNear(lookup.strip1[0].pbRange, 1.0f, 1.0e-6f,
                         midi2Mode
                             ? "MPE master-channel strip pitch bend should keep the full pitch-bend range in MIDI 2.0 mode"
                             : "MPE master-channel strip pitch bend should keep the full pitch-bend range in legacy MIDI mode");

        juce::ignoreUnused(modeName);
    };

    verifyMode(false);
    verifyMode(true);
    return ok;
}

bool verifySingleChannelStripPitchBendKeepsFullRange()
{
    using namespace ecm;

    bool ok = true;
    auto verifyMode = [&](bool midi2Mode) {
        DummyProcessor processor;
        juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
        juce::CriticalSection stateLock;
        ConfigLookup configLookups[] = {
            ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
            ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
            ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
        };

        SettingsWrapper::setMidi2Mode(midi2Mode, pluginState.state);
        ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan1, pluginState.state);
        ZoneWrapper::setChannelMaxPitchbend(InstrumentType::Alpha, Zone::Zone1, 12, pluginState.state);
        ZoneWrapper::setMidiValue(InstrumentType::Alpha,
                                  Zone::Zone1,
                                  ZoneWrapper::id_strip1Abs,
                                  { MidiValueType::Pitchbend, 0 },
                                  pluginState.state);
        ZoneWrapper::setMidiValue(InstrumentType::Alpha,
                                  Zone::Zone1,
                                  ZoneWrapper::id_strip1Rel,
                                  { MidiValueType::Off, 0 },
                                  pluginState.state);
        ZoneWrapper::setMidiValue(InstrumentType::Alpha,
                                  Zone::Zone1,
                                  ZoneWrapper::id_breath,
                                  { MidiValueType::Pitchbend, 0 },
                                  pluginState.state);

        for (auto& lookup : configLookups)
            lookup.updateAll();

        MidiService midiService(configLookups, stateLock);
        midiService.start(pluginState, nullptr);
        midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            configLookups,
            midiService.getProtocol(),
            midiService.getVoiceRouter(),
            midiService.getExpressionPolicy()));

        CapturingSink sink;
        osc::Message outgoingMessage;
        juce::MidiBuffer midiBuffer;
        midiService.processMessage(makeStripMessage(InstrumentType::Alpha, 1, 0.0f), outgoingMessage, midiBuffer, sink, 0, nullptr);

        const PerformanceEvent* stripPitchBend = nullptr;
        for (const auto& event : sink.events)
        {
            if (event.kind == PerformanceEventKind::PitchBend)
                stripPitchBend = &event;
        }

        ok &= expectNear(configLookups[0].breath[0].pbRange, 1.0f, 1.0e-6f,
                         midi2Mode
                             ? "single-channel breath pitch bend should keep the full pitch-bend range in MIDI 2.0 mode"
                             : "single-channel breath pitch bend should keep the full pitch-bend range in legacy MIDI mode");
        ok &= expectNear(configLookups[0].strip1[0].pbRange, 1.0f, 1.0e-6f,
                         midi2Mode
                             ? "single-channel strip pitch bend should keep the full pitch-bend range in MIDI 2.0 mode"
                             : "single-channel strip pitch bend should keep the full pitch-bend range in legacy MIDI mode");
        ok &= expect(stripPitchBend != nullptr,
                     midi2Mode
                         ? "processing a strip message should emit a pitch-bend event in MIDI 2.0 mode"
                         : "processing a strip message should emit a pitch-bend event in legacy MIDI mode");
        if (stripPitchBend != nullptr)
        {
            ok &= expectNear(stripPitchBend->value, 1.0f, 1.0e-6f,
                             midi2Mode
                                 ? "single-channel strip pitch bend should reach the full protocol range in MIDI 2.0 mode"
                                 : "single-channel strip pitch bend should reach the full protocol range in legacy MIDI mode");
            ok &= expect(stripPitchBend->channel == 1,
                         midi2Mode
                             ? "single-channel strip pitch bend should be emitted on the configured zone channel in MIDI 2.0 mode"
                             : "single-channel strip pitch bend should be emitted on the configured zone channel in legacy MIDI mode");
            ok &= expect(!stripPitchBend->perNote,
                         midi2Mode
                             ? "single-channel strip pitch bend should remain channel-wide in MIDI 2.0 mode"
                             : "single-channel strip pitch bend should remain channel-wide in legacy MIDI mode");
        }

        midiService.stop();
    };

    verifyMode(false);
    verifyMode(true);
    return ok;
}

bool verifyMidiMessageKeysEmitMappedMessages()
{
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
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan1, pluginState.state);

    auto exerciseMapping = [&](const juce::String& mappingValue, const Zone zone = Zone::Zone1) {
        auto layoutKey = makeLayoutKey(KeyMappingType::MidiMsg, mappingValue);
        layoutKey.zone = zone;
        LayoutWrapper::setLayoutKey(layoutKey, pluginState.state);

        MidiService midiService(configLookups, stateLock);
        midiService.start(pluginState, nullptr);
        midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            configLookups,
            midiService.getProtocol(),
            midiService.getVoiceRouter(),
            midiService.getExpressionPolicy()));

        osc::Message outgoingMessage;
        juce::MidiBuffer midiBuffer;
        auto message = makeKeyMessage(0.0f, 0.0f, 0.0f, 1'000'000ULL);
        midiService.processMessage(message, outgoingMessage, midiBuffer, 0, nullptr);
        midiService.stop();
        return midiBuffer;
    };

    auto exercisePressAndReleaseMapping = [&](const juce::String& mappingValue, const Zone zone = Zone::Zone1) {
        auto layoutKey = makeLayoutKey(KeyMappingType::MidiMsg, mappingValue);
        layoutKey.zone = zone;
        LayoutWrapper::setLayoutKey(layoutKey, pluginState.state);

        MidiService midiService(configLookups, stateLock);
        midiService.start(pluginState, nullptr);
        midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            configLookups,
            midiService.getProtocol(),
            midiService.getVoiceRouter(),
            midiService.getExpressionPolicy()));

        osc::Message outgoingMessage;
        juce::MidiBuffer midiBuffer;
        auto press = makeKeyMessage(0.0f, 0.0f, 0.0f, 1'000'000ULL);
        midiService.processMessage(press, outgoingMessage, midiBuffer, 0, nullptr);

        auto release = press;
        release.active = false;
        release.timestamp += 1'000ULL;
        midiService.processMessage(release, outgoingMessage, midiBuffer, 0, nullptr);

        midiService.stop();
        return midiBuffer;
    };

    bool ok = true;

    const auto ccBuffer = exerciseMapping("Trigger;CC;74;0;127");
    ok &= expect(containsControllerChange(ccBuffer, 1, 74, 127), "trigger CC command keys should emit the configured controller message on press");

    const auto pcBuffer = exerciseMapping("Trigger;PC;0;0;10");
    ok &= expect(containsProgramChange(pcBuffer, 1, 10), "trigger program-change command keys should emit the configured program change on press");

    const auto allNotesOffBuffer = exerciseMapping("Trigger;AllNotesOff;0;0;0");
    ok &= expect(countAllNotesOffMessages(allNotesOffBuffer) == 16, "all-notes-off command keys should emit all-notes-off on every MIDI channel");

    const auto momentaryBuffer = exercisePressAndReleaseMapping("Momentary;CC;74;12;99");
    ok &= expect(containsControllerChange(momentaryBuffer, 1, 74, 99), "momentary CC command keys should emit the configured on value on press");
    ok &= expect(containsControllerChange(momentaryBuffer, 1, 74, 12), "momentary CC command keys should emit the configured off value on release");

    const auto latchBuffer = exercisePressAndReleaseMapping("Latch;CC;74;12;99");
    ok &= expect(countControllerChangeMessages(latchBuffer, 1, 74, 99) == 1,
                 "latch CC command keys should not emit an extra off message on release");

    const auto noZoneBuffer = exerciseMapping("Trigger;CC;74;0;127", Zone::NoZone);
    ok &= expect(containsControllerChange(noZoneBuffer, 1, 74, 127),
                 "command keys without an explicit zone should still emit their configured MIDI message");

    return ok;
}

} // namespace

int main() {
    using namespace ecm;

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    const bool ok = verifyCenteredNoteOnAndStrideLimitedTransition()
                 && verifyMidi2KeyPitchBendScalingMatchesLegacy()
                 && verifyMpeMasterPitchBendUsesChannelMaxRange()
                 && verifySingleChannelStripPitchBendKeepsFullRange()
                 && verifyMidiMessageKeysEmitMappedMessages();
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "NoteOnExpressionTransitionTest passed" << std::endl;
    return 0;
}