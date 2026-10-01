#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/LayoutWrapper.h"
#include "Core/MidiService.h"
#include "Core/MidiBufferUtils.h"
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

std::vector<std::vector<uint8_t>> collectMidiMessages(const juce::MidiBuffer& buffer)
{
    std::vector<std::vector<uint8_t>> messages;

    for (const auto metadata : buffer)
    {
        auto message = metadata.getMessage();
        const auto* data = message.getRawData();
        const auto size = static_cast<size_t>(message.getRawDataSize());
        messages.emplace_back(data, data + size);
    }

    return messages;
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

ecm::osc::Message makeKeyMessageForKey(unsigned int keyNo, float pressure, float roll, float yaw, uint64_t timestamp) {
    auto message = makeKeyMessage(pressure, roll, yaw, timestamp);
    message.key = keyNo;
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
    key.notes = { 60, -1, -1, -1, -1, -1 };
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

bool verifyMpePitchbendEditUpdatesTransportWithoutResettingVoices()
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
    SettingsWrapper::setLowerMPEVoiceCount(5, pluginState.state);
    SettingsWrapper::setUpperMPEVoiceCount(3, pluginState.state);
    SettingsWrapper::setLowerMPEPB(48, pluginState.state);
    SettingsWrapper::setUpperMPEPB(48, pluginState.state);

    auto firstLayoutKey = makeLayoutKey(KeyMappingType::Note, "60");
    LayoutWrapper::setLayoutKey(firstLayoutKey, pluginState.state);

    auto secondLayoutKey = makeLayoutKey(KeyMappingType::Note, "62");
    secondLayoutKey.keyId.keyNo = 1;
    LayoutWrapper::setLayoutKey(secondLayoutKey, pluginState.state);

    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::MPE_Low, pluginState.state);

    for (auto& lookup : configLookups)
        lookup.updateAll();

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        configLookups,
        midiService.getProtocol(),
        midiService.getVoiceRouter(),
        midiService.getExpressionPolicy()));

    juce::MidiBuffer startupMessages;
    midiService.drainPendingMidiMessages(startupMessages, 0);

    const uint64_t startTimestamp = 1'000'000ULL;
    osc::Message outgoingMessage;
    juce::MidiBuffer midiBuffer;

    auto sendNoteOn = [&](unsigned int keyNo, int noteNo, uint64_t timestampBase) {
        CapturingSink sink;
        for (int i = 0; i < 6; ++i)
        {
            auto message = makeKeyMessageForKey(keyNo, 0.7f, 0.0f, 0.0f, timestampBase + static_cast<uint64_t>(i) * 10'000ULL);
            midiService.processMessage(message, outgoingMessage, midiBuffer, sink, 0, nullptr);
        }

        const PerformanceEvent* noteOn = nullptr;
        for (const auto& event : sink.events)
        {
            if (event.kind == PerformanceEventKind::NoteOn && event.noteNumber == noteNo)
            {
                noteOn = &event;
                break;
            }
        }

        return noteOn != nullptr ? noteOn->channel : -1;
    };

    const int firstChannel = sendNoteOn(0, 60, startTimestamp);

    // Keep a voice active across the edit to detect an accidental allocator reset.
    SettingsWrapper::setLowerMPEPB(12, pluginState.state);

    juce::MidiBuffer queuedMessages;
    midiService.drainPendingMidiMessages(queuedMessages, 0);

    const auto presetTreeAfterPitchbendEdit = SettingsWrapper::getPresetTree(pluginState.state);
    const bool presetHasNestedPreset = presetTreeAfterPitchbendEdit.getChildWithName(SettingsWrapper::id_preset).isValid();
    const bool alphaLayoutRemainsInPreset = presetTreeAfterPitchbendEdit
        .getChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Alpha))
        .getChildWithName(LayoutWrapper::id_layout)
        .getChildWithName(LayoutWrapper::id_key + juce::String("_0_0"))
        .isValid();

    const int secondChannel = sendNoteOn(1, 62, startTimestamp + 100'000ULL);

    juce::MidiBuffer refreshedLayout;
    midiService.createLayoutRPNs(refreshedLayout);

    const auto refreshedMessages = collectMidiMessages(refreshedLayout);
    const auto queuedMidiMessages = collectMidiMessages(queuedMessages);

    bool ok = true;
    ok &= expect(!queuedMessages.isEmpty(),
                 "changing lower MPE pitch bend should immediately queue the updated transport layout");
    juce::MPEZoneLayout queuedLayout;
    queuedLayout.processNextMidiBuffer(queuedMessages);
    ok &= expect(queuedLayout.getLowerZone().perNotePitchbendRange == 12,
                 "the queued layout should advertise the edited lower-zone pitch-bend range");
    ok &= expect(queuedLayout.getLowerZone().numMemberChannels == 5
                 && queuedLayout.getUpperZone().numMemberChannels == 3
                 && queuedLayout.getUpperZone().perNotePitchbendRange == 48,
                 "a lower-zone pitch-bend edit should preserve voice counts and the upper-zone range");
    ok &= expect(!presetHasNestedPreset,
                 "changing lower MPE pitch bend through a MidiService listener should not create a nested preset subtree");
    ok &= expect(alphaLayoutRemainsInPreset,
                 "changing lower MPE pitch bend through a MidiService listener should keep layout keys in the root preset subtree");
    ok &= expect(LayoutWrapper::getLayoutKey({ 0, 0, InstrumentType::Alpha }, pluginState.state).mappingValue == "60",
                 "changing lower MPE pitch bend through a MidiService listener should not clear mapped keys to default C-2 values");
    ok &= expect(firstChannel == 2,
                 "the first active lower-zone MPE note should still use the first member channel");
    ok &= expect(secondChannel == 3,
                 "changing lower MPE pitch bend should not reset active lower-zone voice allocation before the next note");
    ok &= expect(!refreshedMessages.empty(),
                 "rebuilding the transport layout after an MPE pitch-bend edit should still produce layout messages");
    ok &= expect(refreshedMessages == queuedMidiMessages,
                 "the queued update should match the current transport layout definition");

    midiService.stop();
    return ok;
}

bool verifyStrummedHammerOns(bool midi2Mode = false, bool keyBendEnabled = true, bool nativePerNote = false)
{
    using namespace ecm;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup lookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };
    SettingsWrapper::setMidi2Mode(midi2Mode, pluginState.state);
    SettingsWrapper::setLowerMPEVoiceCount(6, pluginState.state);
    SettingsWrapper::setLowerMPEPB(12, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::MPE_Low, pluginState.state);
    ZoneWrapper::setKeyPitchbend(InstrumentType::Alpha, Zone::Zone2, 2, pluginState.state);
    ZoneWrapper::setMidiValue(InstrumentType::Alpha, Zone::Zone2, ZoneWrapper::id_roll,
                             {keyBendEnabled ? MidiValueType::Pitchbend : MidiValueType::Off, 0}, pluginState.state);
    ZoneWrapper::setMidiValue(InstrumentType::Alpha, Zone::Zone2, ZoneWrapper::id_yaw,
                             {MidiValueType::CC, 74}, pluginState.state);
    bool ok = expect(LayoutWrapper::getLayoutKey({0, 0, InstrumentType::Alpha}, pluginState.state).stringNumber == 0,
                     "existing layouts should default to no string");
    const int pitches[] = {60, 64, 70, 71, 55, 48, 62, 63, 60, 67};
    for (int k = 0; k < 10; ++k) {
        auto key = makeLayoutKey(k == 5 ? KeyMappingType::Chord : KeyMappingType::Note,
                                 k == 5 ? "Chord;48;52;55;-1" : juce::String(pitches[k]));
        key.keyId.keyNo = k;
        key.stringNumber = k == 6 ? 2 : k == 7 ? 0 : 1;
        if (k == 9) key.zone = Zone::Zone2;
        LayoutWrapper::setLayoutKey(key, pluginState.state);
    }
    auto panic = makeLayoutKey(KeyMappingType::MidiMsg, "Trigger;AllNotesOff;0;0;0");
    panic.keyId.keyNo = 10;
    LayoutWrapper::setLayoutKey(panic, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone2, MidiChannelType::MPE_Low, pluginState.state);
    ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone1, false, pluginState.state);
    for (int string = 1; string <= 2; ++string) {
        auto strum = makeLayoutKey(KeyMappingType::Strum, "Strum;1;" + juce::String(string) + ";0");
        strum.keyId.keyNo = 10 + string;
        strum.zone = Zone::Zone2;
        LayoutWrapper::setLayoutKey(strum, pluginState.state);
        LayoutWrapper::setStrumSettings(strum.keyId, {60, 0, false}, pluginState.state);
    }
    auto saved = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
    auto restored = juce::ValueTree::fromXml(*saved.createXml());
    ok &= expect(int(restored.getChildWithName("key_0_1").getProperty(LayoutWrapper::id_stringNumber)) == 1,
                 "note string assignments should remain persistent");
    for (auto& lookup : lookups) lookup.updateAll();
    MidiService service(lookups, stateLock);
    service.start(pluginState, nullptr);
    if (nativePerNote) {
        // Receiving a MIDI 2.0 channel-voice packet enables negotiated per-note expression.
        const uint32_t packet[] = { 0x40d00000u, 0u };
        using juce::universal_midi_packets::Iterator;
        service.consume(Iterator(packet, 2), Iterator(packet + 2, 0), 0);
    }
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
    CapturingSink sink;
    osc::Message outgoing;
    juce::MidiBuffer buffer;
    uint64_t time = 1'000'000;
    auto send = [&](unsigned int key, bool active, int count, float yaw = 0.0f, float roll = 0.0f) {
        sink.events.clear();
        for (int i = 0; i < count; ++i) {
            auto msg = makeKeyMessageForKey(key, 0.7f, roll, yaw, time += 10'000);
            msg.active = active;
            service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        }
    };
    auto press = [&](unsigned int key) {
        send(key, true, 6);
        for (const auto& event : sink.events)
            if (event.kind == PerformanceEventKind::NoteOn) return event.channel;
        return -1;
    };
    auto release = [&](unsigned int key) { send(key, false, 1); };
    auto countKind = [&](PerformanceEventKind kind) {
        int count = 0;
        for (const auto& event : sink.events) if (event.kind == kind) ++count;
        return count;
    };
    int originalNote = 60;
    auto expectBend = [&](float semitones) {
        const PerformanceEvent* bend = nullptr;
        for (const auto& event : sink.events)
            if (event.kind == PerformanceEventKind::PitchBend) bend = &event;
        ok &= expect(bend != nullptr, "string ownership changes should immediately emit pitch bend");
        if (bend && nativePerNote)
            ok &= expect(bend->perNote && bend->noteNumber == originalNote,
                         "native expression must target the original sounding MIDI note");
        if (bend) ok &= expectNear(bend->value, 0.5f + semitones / 24.0f, 0.0001f,
                                  "string bend should be relative to the original MIDI note");
    };
    press(0);
    ok &= expect(sink.events.empty(), "fretting a silent source must not start a voice");
    const int first = press(11);
    ok &= expect(nativePerNote ? first == 1 : first > 1, "the strum must use its output zone voice routing");
    release(11);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "source-owned strum must sustain after key release");
    press(1);
    ok &= expect(countKind(PerformanceEventKind::NoteOn) == 0, "hammer-on must bend without retriggering");
    expectBend(4);
    send(0, true, 80, 0.4f);
    ok &= expect(sink.events.empty(), "the older fret must not fight the new fret for expression");
    send(1, true, 80, 0.2f);
    ok &= expect(findControllerEvent(sink.events, 74) != nullptr, "hammered fret must supply linked expression");
    expectBend(4);
    press(4);
    expectBend(-5);
    release(4);
    expectBend(4);
    release(1);
    expectBend(0);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "pull-off must preserve the original strummed voice");
    press(2);
    expectBend(10);
    send(2, true, 80, 0, 1);
    if (keyBendEnabled) {
        for (const auto& event : sink.events)
            if (event.kind == PerformanceEventKind::PitchBend)
                ok &= expect(event.value > 0.99f && event.value <= 1.0f, "hammer-on must reserve room for fret expression");
    } else expectBend(10);
    press(3);
    ok &= expect(sink.events.empty(), "an out-of-range fret must not clip the sounding pitch");
    release(3);
    ok &= expect(sink.events.empty(), "an out-of-range fret release must not change pitch");
    press(6);
    ok &= expect(sink.events.empty(), "another string must not bend this strummed voice");
    const int other = press(12);
    ok &= expect(nativePerNote ? other == 1 : other != first && other > 1, "another string must retain independent routing");
    release(12);
    release(6);
    release(0);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "releasing the original fret must retain a hammered voice");
    release(2);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 1, "last fret release must end a source-owned strum");
    for (const auto& event : sink.events)
        if (event.kind == PerformanceEventKind::NoteOff)
            ok &= expect(event.noteNumber == originalNote && event.channel == first, "Note Off must use the original pitch and channel");

    // An open strum with key release disabled survives hammer-ons and pull-offs.
    press(11);
    release(11);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "open strum must sustain after its key is released when Controls note off is disabled");
    press(6);
    release(6);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "unrelated key releases must not end a sustained open strum");
    press(1);
    expectBend(4);
    release(1);
    expectBend(0);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "pulling off the final fret must retain a voice started as an open strum");

    // Re-strumming after a hammer-on must allocate the new fretted pitch, with no residual bend.
    press(0);
    press(11);
    release(11);
    press(1);
    expectBend(4);
    originalNote = 64;
    press(11);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == 1 && countKind(PerformanceEventKind::NoteOn) == 1,
                 "re-strum must replace the bent voice");
    expectBend(0);
    release(11);
    release(1);
    expectBend(-4);
    release(0);

    // String assignment no longer changes how the fret zone itself plays notes.
    ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone1, true, pluginState.state);
    for (auto& lookup : lookups) lookup.updateAll();
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
    const int fretChannel = press(0);
    const int secondFretChannel = press(1);
    ok &= expect(countKind(PerformanceEventKind::NoteOn) == 1
                 && (nativePerNote || fretChannel != secondFretChannel), "frets on the same string must play independently on their own output");
    press(3);
    ok &= expect(countKind(PerformanceEventKind::NoteOn) == 1, "fret-zone notes must not be restricted by a string bend range");
    release(3);
    release(1);
    release(0);
    service.stop();
    return ok;
}

bool verifyStrummingAndSixNoteChords(bool silentSource = false, bool strumSettings = false, bool releaseModes = false, bool palmMute = false)
{
    using namespace ecm;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup lookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };
    SettingsWrapper::setMidi2Mode(false, pluginState.state);
    ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone1, !silentSource, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan1, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone2, MidiChannelType::Chan2, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone3, MidiChannelType::Chan3, pluginState.state);
    ZoneWrapper::setChannelMaxPitchbend(InstrumentType::Alpha, Zone::Zone1, 48, pluginState.state);
    auto map = [&](int keyNo, KeyMappingType type, const juce::String& value, Zone zone, int string = 0) {
        auto key = makeLayoutKey(type, value);
        key.keyId.keyNo = keyNo;
        key.zone = zone;
        key.stringNumber = string;
        LayoutWrapper::setLayoutKey(key, pluginState.state);
    };
    map(0, KeyMappingType::Note, "60", Zone::Zone1, 1);
    map(1, KeyMappingType::Note, "64", Zone::Zone1, 1);
    map(2, KeyMappingType::Chord, "Six;48;52;55;60;64;67", Zone::Zone1);
    map(3, KeyMappingType::Chord, "Old;50;53;57;62", Zone::Zone1);
    map(4, KeyMappingType::Note, "80", Zone::Zone3, 12);
    map(5, KeyMappingType::Chord, "DA;62;69;-1;-1", Zone::Zone1);
    map(6, KeyMappingType::Chord, "FED;65;64;62;-1", Zone::Zone1);
    map(7, KeyMappingType::Chord, "Upper;70;72;74;76;77;79", Zone::Zone1);
    for (int string = 1; string <= 12; ++string)
        map(19 + string, KeyMappingType::Strum, "Strum;1;" + juce::String(string), Zone::Zone2);
    map(10, KeyMappingType::Strum, "Strum;1;1", Zone::Zone2);
    map(11, KeyMappingType::Strum, "Strum;1;6", Zone::Zone2);
    map(12, KeyMappingType::Note, "75", Zone::Zone2);
    map(13, KeyMappingType::Strum, "Strum;3;12", Zone::Zone2);
    for (auto& lookup : lookups) lookup.updateAll();
    bool ok = expect(lookups[0].keys[0][2].notes[5] == 67, "six-note chords should preserve the sixth slot");
    ok &= expect(lookups[0].keys[0][3].notes[4] == -1 && lookups[0].keys[0][3].notes[5] == -1,
                 "legacy four-note chords should leave strings five and six empty");
    ok &= expect(LayoutWrapper::getLayoutKey({0, 4, InstrumentType::Alpha}, pluginState.state).stringNumber == 12,
                 "note string twelve should survive layout storage");
    ok &= expect(lookups[0].keys[0][31].strumSourceString == 12,
                 "strum string twelve should survive configuration lookup");
    auto layout = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
    auto restored = juce::ValueTree::fromXml(*layout.createXml());
    ok &= expect(restored.getChildWithName("key_0_11").getProperty(LayoutWrapper::id_mappingValue).toString() == "Strum;1;6",
                 "strum links should persist with the layout");
    MidiService service(lookups, stateLock);
    service.start(pluginState, nullptr);
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
    CapturingSink sink;
    osc::Message outgoing;
    juce::MidiBuffer buffer;
    uint64_t time = 1'000'000;
    auto press = [&](unsigned int key, float strength = 0.1f) {
        sink.events.clear();
        for (int i = 0; i < 6; ++i) {
            auto message = makeKeyMessageForKey(key, strength * static_cast<float>(i + 1), 0, 0, time += 10'000);
            service.processMessage(message, outgoing, buffer, sink, 0, nullptr);
        }
    };
    auto release = [&](unsigned int key) {
        sink.events.clear();
        auto message = makeKeyMessageForKey(key, 0, 0, 0, time += 10'000);
        message.active = false;
        service.processMessage(message, outgoing, buffer, sink, 0, nullptr);
    };
    auto noteEvent = [&](PerformanceEventKind kind, int channel, int note) {
        PerformanceEvent result;
        bool found = false;
        for (const auto& event : sink.events)
            if (event.kind == kind && event.channel == channel && event.noteNumber == note) {
                result = event;
                found = true;
            }
        ok &= expect(found, "expected note event should use the strum's output channel and linked source pitch");
        return result;
    };
    auto countKind = [&](PerformanceEventKind kind) {
        int result = 0;
        for (const auto& event : sink.events) if (event.kind == kind) ++result;
        return result;
    };
    if (palmMute) {
        map(40, KeyMappingType::PalmMute, "PalmMute;Latch", Zone::Zone2);
        map(41, KeyMappingType::PalmMute, "PalmMute;Momentary", Zone::Zone2);
        map(42, KeyMappingType::PalmMute, "PalmMute;Momentary", Zone::Zone2);
        map(43, KeyMappingType::PalmMute, "PalmMute;Latch", Zone::Zone1);
        map(44, KeyMappingType::Chord, "Pair;70;72;-1;-1", Zone::Zone2);
        LayoutWrapper::setStrumSettings({0, 10, InstrumentType::Alpha}, {40, 9, false}, pluginState.state);
        for (auto& lookup : lookups) lookup.updateAll();
        service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        ok &= expect(!lookups[0].keys[0][40].palmMuteMomentary && lookups[0].keys[0][41].palmMuteMomentary,
                     "palm mute must distinguish latch and momentary mappings");
        const auto savedMute = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
        const auto restoredMute = juce::ValueTree::fromXml(*savedMute.createXml());
        ok &= expect(restoredMute.getChildWithName("key_0_41").getProperty(LayoutWrapper::id_mappingValue).toString() == "PalmMute;Momentary",
                     "palm mute mode must persist in layouts");
        auto mutedStrike = [&](int pitch, double sampleRate = 48000.0, int blockSize = 128) {
            service.beginPerformanceBlock(sampleRate);
            press(10);
            noteEvent(PerformanceEventKind::NoteOn, 9, pitch);
            ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "muted notes need time to trigger before Note Off");
            release(10);
            ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "an early physical release must preserve the short muted note");
            service.endPerformanceBlock(buffer, blockSize, &sink);
            ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "muted Note On and Off must not share an output block");
            int elapsed = blockSize;
            const int due = static_cast<int>(std::round(sampleRate * 0.010));
            for (;;) {
                sink.events.clear();
                service.beginPerformanceBlock(sampleRate);
                service.endPerformanceBlock(buffer, blockSize, &sink);
                if (elapsed + blockSize > due) {
                    const auto off = noteEvent(PerformanceEventKind::NoteOff, 9, pitch);
                    ok &= expect(off.sampleOffset == std::max(0, due - elapsed), "muted note must release at 10 ms or the next output block");
                    break;
                }
                ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "muted note must not release before its deadline");
                elapsed += blockSize;
            }
            sink.events.clear();
            service.beginPerformanceBlock(sampleRate);
            service.endPerformanceBlock(buffer, blockSize, &sink);
            ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "scheduled release must fire only once");
        };
        press(0);
        press(4);
        press(10);
        release(10);
        press(12);
        press(44);
        press(40);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        noteEvent(PerformanceEventKind::NoteOff, 2, 75);
        noteEvent(PerformanceEventKind::NoteOff, 2, 70);
        noteEvent(PerformanceEventKind::NoteOff, 2, 72);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 4, "activation must stop every note in its zone and leave other zones sounding");
        mutedStrike(60);
        mutedStrike(60, 96000.0);
        mutedStrike(60, 48000.0, 1024);
        release(40);
        mutedStrike(60);
        service.beginPerformanceBlock(48000.0);
        press(10);
        release(10);
        service.endPerformanceBlock(buffer, 128, &sink);
        service.beginPerformanceBlock(48000.0);
        press(10);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        release(10);
        service.endPerformanceBlock(buffer, 128, &sink);
        sink.events.clear();
        service.beginPerformanceBlock(48000.0);
        service.endPerformanceBlock(buffer, 256, &sink);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "an old deadline must not end a replacement strum");
        service.beginPerformanceBlock(48000.0);
        service.endPerformanceBlock(buffer, 128, &sink);
        const auto replacementOff = noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        ok &= expect(replacementOff.sampleOffset == 96, "replacement strum must get its own full note length");
        release(12);
        release(44);
        press(12);
        noteEvent(PerformanceEventKind::NoteOn, 2, 75);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "palm mute shortens strums, not newly played ordinary notes");
        release(12);
        press(40);
        release(40);
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "second latch press must restore sustained strums");
        release(10);
        release(0);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        release(4);
        noteEvent(PerformanceEventKind::NoteOff, 3, 80);

        press(41);
        press(42);
        release(41);
        mutedStrike(40);
        release(42);
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 9, 40);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "last momentary release must restore open-note sustain");
        release(10);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "open strums with key release disabled must keep sounding");
        press(41);
        noteEvent(PerformanceEventKind::NoteOff, 9, 40);
        release(41);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "palm mute must release a sustained open strum only once");

        // Stopping a source zone must keep its held fingering available to a different output zone.
        press(0);
        press(43);
        if (!silentSource) noteEvent(PerformanceEventKind::NoteOff, 1, 60);
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "source-zone palm mute must not mute a different strum output zone");
        release(10);
        release(0);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        release(43);
        auto refreshMute = [&] {
            for (auto& lookup : lookups) lookup.updateAll();
            service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
                lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        };
        ok &= expect(LayoutWrapper::getPalmMutePressureCC({0, 41, InstrumentType::Alpha}, pluginState.state) == -1,
                     "palm pressure CC must be optional and off by default");
        LayoutWrapper::setPalmMutePressureCC({0, 41, InstrumentType::Alpha}, 74, pluginState.state);
        refreshMute();
        const auto savedCC = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
        const auto restoredCC = juce::ValueTree::fromXml(*savedCC.createXml());
        ok &= expect(int(restoredCC.getChildWithName("key_0_41").getProperty("palmMutePressureCC", -1)) == 74,
                     "palm pressure CC must persist with the layout");
        auto mutePressure = [&](unsigned int key, float pressure) {
            sink.events.clear();
            auto msg = makeKeyMessageForKey(key, pressure, 0, 0, time += 10'000);
            msg.active = true;
            service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        };
        float mutedReleaseVelocity = -1.0f;
        auto measureMutedStrike = [&] {
            service.beginPerformanceBlock(48000.0);
            press(10);
            release(10);
            service.endPerformanceBlock(buffer, 128, &sink);
            for (int elapsed = 128; elapsed < 4096; elapsed += 128) {
                sink.events.clear();
                service.beginPerformanceBlock(48000.0);
                service.endPerformanceBlock(buffer, 128, &sink);
                for (const auto& event : sink.events)
                    if (event.kind == PerformanceEventKind::NoteOff && event.channel == 9) {
                        mutedReleaseVelocity = event.velocity;
                        return elapsed + event.sampleOffset;
                    }
            }
            ok &= expect(false, "pressure-shaped mute must end within its short duration range");
            return -1;
        };
        mutePressure(41, 0.01f);
        const auto* softCC = findControllerEvent(sink.events, 74);
        ok &= expect(softCC && softCC->channel == 2 && softCC->zoneIndex == 1 && softCC->value > 0 && softCC->value < 0.1f,
                     "soft palm pressure must send a low CC on the selected zone channel");
        const int softLength = measureMutedStrike();
        const float softReleaseVelocity = mutedReleaseVelocity;
        mutePressure(41, 1.0f);
        const auto* hardCC = findControllerEvent(sink.events, 74);
        ok &= expect(hardCC && std::abs(hardCC->value - 1.0f) < 0.000001f, "firm palm pressure must send full-scale CC");
        const int hardLength = measureMutedStrike();
        ok &= expect(hardLength == 480 && softLength > 3000 && softLength <= 3840,
                     "soft mute must ring longer than hard mute within 10-80 ms");
        ok &= expect(softReleaseVelocity > 0.0f && softReleaseVelocity < 0.1f
                     && mutedReleaseVelocity > 0.99f, "firmer palm pressure must produce higher note-off velocity");
        mutePressure(41, 1.0f);
        ok &= expect(findControllerEvent(sink.events, 74) == nullptr, "unchanged pressure must not repeat identical CC messages");
        release(41);
        const auto* zeroCC = findControllerEvent(sink.events, 74);
        ok &= expect(zeroCC && zeroCC->value == 0.0f, "pressure CC must return to zero on key release");
        for (const auto output : {MidiChannelType::MPE_Low, MidiChannelType::MPE_High}) {
            ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone2, output, pluginState.state);
            refreshMute();
            mutePressure(41, 0.2f);
            const auto* mpeCC = findControllerEvent(sink.events, 74);
            ok &= expect(mpeCC && mpeCC->channel == (output == MidiChannelType::MPE_Low ? 1 : 16) && !mpeCC->perNote,
                         "palm pressure CC must use the MPE master channel");
            release(41);
        }
        LayoutWrapper::setPalmMutePressureCC({0, 41, InstrumentType::Alpha}, -1, pluginState.state);
        refreshMute();
        mutePressure(41, 0.2f);
        ok &= expect(findControllerEvent(sink.events, 74) == nullptr, "disabled pressure CC must not emit controller messages");
        release(41);
        // Latch ignores pressure and configured CC, including after release.
        LayoutWrapper::setPalmMutePressureCC({0, 40, InstrumentType::Alpha}, 74, pluginState.state);
        refreshMute();
        mutePressure(40, 0.01f);
        ok &= expect(findControllerEvent(sink.events, 74) == nullptr, "latch must not send pressure CC on press");
        ok &= expect(measureMutedStrike() == 480, "soft latch press must use a fixed 10 ms mute");
        release(40);
        ok &= expect(findControllerEvent(sink.events, 74) == nullptr, "latch must not send pressure CC on release");
        ok &= expect(measureMutedStrike() == 480, "latched mute must keep a fixed 10 ms after release");
        press(40);
        release(40);
        service.stop();
        return ok;
    }
    if (releaseModes) {
        auto refresh = [&] {
            for (auto& lookup : lookups) lookup.updateAll();
            service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
                lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        };
        auto configure = [&](unsigned int key, bool controlsOff) {
            LayoutWrapper::setStrumSettings({0, static_cast<int>(key), InstrumentType::Alpha}, {40, 9, controlsOff}, pluginState.state);
            refresh();
        };
        configure(10, false);
        configure(20, false);
        const auto saved = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
        const auto restoredReleaseSettings = juce::ValueTree::fromXml(*saved.createXml());
        ok &= expect(!static_cast<bool>(restoredReleaseSettings.getChildWithName("key_0_10").getProperty("controlsNoteOff", true)),
                     "note-off ownership must survive layout serialization");
        press(0);
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        release(10);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "source-owned strum must survive strum release");
        press(1);
        release(0);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == (silentSource ? 0 : 1), "remaining finger must retain the strummed voice");
        release(1);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        ok &= expect(countKind(PerformanceEventKind::NoteOn) == 0, "releasing the string must not automatically play an open note");

        press(0);
        press(10);
        release(10);
        press(10);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        release(10);
        press(1);
        press(20);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        noteEvent(PerformanceEventKind::NoteOn, 9, 64);
        int offIndex = -1, onIndex = -1;
        for (size_t i = 0; i < sink.events.size(); ++i) {
            if (sink.events[i].kind == PerformanceEventKind::NoteOff) offIndex = static_cast<int>(i);
            if (sink.events[i].kind == PerformanceEventKind::NoteOn) onIndex = static_cast<int>(i);
        }
        ok &= expect(offIndex >= 0 && onIndex > offIndex, "replacement must end the old pitch before starting the new pitch");
        release(0);
        release(1);
        noteEvent(PerformanceEventKind::NoteOff, 9, 64);
        release(20);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "a source-ended voice must not end twice");

        configure(10, true);
        configure(20, true);
        press(0);
        press(10);
        press(20);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        release(20);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        release(10);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "older held strum must not retain or release its replacement");
        press(10);
        release(0);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == (silentSource ? 0 : 1), "key-owned strum must survive source release");
        release(10);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);

        configure(10, false);
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 9, 40);
        press(0);
        release(10);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "strum release must not stop an open-origin voice after hammering");
        release(0);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == (silentSource ? 0 : 1), "fret release must not stop an open-origin voice");
        // Ownership is captured, even when the toggle changes during a note.
        press(0);
        press(10);
        noteEvent(PerformanceEventKind::NoteOff, 9, 40);
        noteEvent(PerformanceEventKind::NoteOn, 9, 60);
        configure(10, true);
        release(10);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == 0, "editing the toggle must not transfer a sounding note's ownership");
        release(0);
        noteEvent(PerformanceEventKind::NoteOff, 9, 60);
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 9, 40);
        release(10);
        noteEvent(PerformanceEventKind::NoteOff, 9, 40);
        configure(10, false);
        press(2);
        press(10);
        release(10);
        press(3);
        release(2);
        ok &= expect(countKind(PerformanceEventKind::NoteOff) == (silentSource ? 0 : 6),
                     "a remaining held chord must retain the strummed voice");
        release(3);
        noteEvent(PerformanceEventKind::NoteOff, 9, 48);
        press(0);
        press(10);
        release(10);
        juce::MidiBuffer pending;
        service.drainPendingMidiMessages(pending);
        service.queueTransposeChangeFlush(InstrumentType::Alpha, Zone::Zone1);
        juce::MidiBuffer flushed;
        service.drainPendingMidiMessages(flushed);
        bool strumOff = false;
        for (const auto metadata : flushed) {
            const auto message = metadata.getMessage();
            if (message.isNoteOff() && message.getChannel() == 9 && message.getNoteNumber() == 60) strumOff = true;
        }
        ok &= expect(strumOff, "flushing a source zone must release its sustained strums");
        service.stop();
        return ok;
    }
    if (strumSettings) {
        LayoutWrapper::setStrumSettings({0, 10, InstrumentType::Alpha}, {40, 9}, pluginState.state);
        auto persisted = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
        const auto restoredSettings = juce::ValueTree::fromXml(*persisted.createXml());
        LayoutWrapper::getLayoutTree(InstrumentType::Alpha, pluginState.state).copyPropertiesAndChildrenFrom(restoredSettings, nullptr);
        const auto settings = LayoutWrapper::getStrumSettings({0, 10, InstrumentType::Alpha}, pluginState.state);
        ok &= expect(settings.openNote == 40 && settings.midiChannel == 9, "strum settings must persist in layouts");
        for (const auto output : {MidiChannelType::Chan2, MidiChannelType::MPE_Low, MidiChannelType::MPE_High}) {
            ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone2, output, pluginState.state);
            for (auto& lookup : lookups) lookup.updateAll();
            ok &= expect(lookups[0].keys[0][0].output == MidiChannelType::Chan1
                         && lookups[0].keys[0][1].output == MidiChannelType::Chan1,
                         "strum settings must not override source note routing");
            ok &= expect(lookups[0].keys[0][4].output == MidiChannelType::Chan3,
                         "strum settings must not affect other zones");
            ok &= expect(lookups[0].keys[0][20].output == output && lookups[0].keys[0][20].openStringNote == -1,
                         "strum keys linked to the same string must keep independent settings");
            service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
                lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
            press(10);
            noteEvent(PerformanceEventKind::NoteOn, 9, 40);
            release(10);
            noteEvent(PerformanceEventKind::NoteOff, 9, 40);
            press(0);
            if (!silentSource) noteEvent(PerformanceEventKind::NoteOn, 1, 60);
            press(10);
            noteEvent(PerformanceEventKind::NoteOn, 9, 60);
            release(10);
            noteEvent(PerformanceEventKind::NoteOff, 9, 60);
            release(0);
            if (!silentSource) noteEvent(PerformanceEventKind::NoteOff, 1, 60);
        }
        // MIDI note zero is a valid open note; linked expression has no source here.
        LayoutWrapper::setKeyMappingValue({0, 10, InstrumentType::Alpha}, "Strum;1;1;0", pluginState.state);
        LayoutWrapper::setStrumSettings({0, 10, InstrumentType::Alpha}, {0, 16}, pluginState.state);
        for (auto& lookup : lookups) lookup.updateAll();
        service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        press(10);
        noteEvent(PerformanceEventKind::NoteOn, 16, 0);
        release(10);
        noteEvent(PerformanceEventKind::NoteOff, 16, 0);
        LayoutWrapper::setStrumSettings({0, 10, InstrumentType::Alpha}, {}, pluginState.state);
        for (auto& lookup : lookups) lookup.updateAll();
        ok &= expect(lookups[0].keys[0][10].output == MidiChannelType::MPE_High,
                     "disabling forced channel must restore zone routing");
        service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        press(10);
        ok &= expect(sink.events.empty(), "disabling open note must restore silent unfretted strums");
        release(10);
        service.stop();
        return ok;
    }
    press(10);
    ok &= expect(sink.events.empty(), "strumming without a held source should be silent");
    release(10);
    press(0, 0.02f);
    const auto sourceOn = silentSource ? PerformanceEvent {} : noteEvent(PerformanceEventKind::NoteOn, 1, 60);
    if (silentSource) ok &= expect(sink.events.empty(), "disabled source notes should remain silent");
    press(10);
    const auto strumOn = noteEvent(PerformanceEventKind::NoteOn, 2, 60);
    ok &= expect(strumOn.zoneIndex == 1, "strum output should carry its own zone routing");
    press(1);
    ok &= expect(countKind(PerformanceEventKind::NoteOn) == (silentSource ? 0 : 1), "fretting should only start the independent source-zone note");
    release(0);
    release(1);
    release(10);
    const auto strumOff = noteEvent(PerformanceEventKind::NoteOff, 2, 60);
    press(12);
    const auto referenceOn = noteEvent(PerformanceEventKind::NoteOn, 2, 75);
    release(12);
    const auto referenceOff = noteEvent(PerformanceEventKind::NoteOff, 2, 75);
    ok &= expectNear(strumOn.velocity, referenceOn.velocity, 0.0001f, "strum velocity should come from its own strike");
    ok &= expectNear(strumOff.velocity, referenceOff.velocity, 0.0001f, "strum release velocity should come from its own pressure history");
    ok &= expect(strumOn.velocity > sourceOn.velocity, "a harder strum should not inherit the softer source velocity");
    press(0);
    press(1);
    press(10);
    noteEvent(PerformanceEventKind::NoteOn, 2, 64);
    release(10);
    release(1);
    press(10);
    noteEvent(PerformanceEventKind::NoteOn, 2, 60);
    release(10);
    release(0);
    press(2);
    ok &= expect(countKind(PerformanceEventKind::NoteOn) == (silentSource ? 0 : 6), "six-note chords should sound only when their own zone is enabled");
    press(11);
    noteEvent(PerformanceEventKind::NoteOn, 2, 67);
    release(11);
    press(10);
    noteEvent(PerformanceEventKind::NoteOn, 2, 48);
    release(10);
    press(3);
    press(11);
    noteEvent(PerformanceEventKind::NoteOn, 2, 57); // Sixth pitch in the combined ascending union.
    release(11);
    press(26);
    noteEvent(PerformanceEventKind::NoteOn, 2, 60); // Seventh distinct pitch.
    release(26);
    noteEvent(PerformanceEventKind::NoteOff, 2, 60);
    press(27);
    noteEvent(PerformanceEventKind::NoteOn, 2, 62); // Eighth distinct pitch.
    release(27);
    noteEvent(PerformanceEventKind::NoteOff, 2, 62);
    release(3);
    press(27);
    ok &= expect(sink.events.empty(), "string eight should be silent when fewer pitches remain");
    release(27);
    press(11);
    noteEvent(PerformanceEventKind::NoteOn, 2, 67);
    release(11);
    press(4);
    press(10);
    noteEvent(PerformanceEventKind::NoteOn, 2, 48);
    release(10);
    press(13);
    noteEvent(PerformanceEventKind::NoteOn, 2, 80);
    release(13);
    release(4);
    release(2);
    ok &= expect(countKind(PerformanceEventKind::NoteOff) == (silentSource ? 0 : 6), "chord release should stop only notes that were actually emitted");
    press(11);
    ok &= expect(sink.events.empty(), "released chords must no longer supply strum pitches");
    release(11);
    press(2);
    press(7);
    const int twelvePitches[] = {48, 52, 55, 60, 64, 67, 70, 72, 74, 76, 77, 79};
    for (unsigned int string = 0; string < 12; ++string) {
        press(20 + string);
        noteEvent(PerformanceEventKind::NoteOn, 2, twelvePitches[string]);
        release(20 + string);
        noteEvent(PerformanceEventKind::NoteOff, 2, twelvePitches[string]);
    }
    release(7);
    press(31);
    ok &= expect(sink.events.empty(), "string twelve should be silent when fewer pitches remain");
    release(31);
    release(2);
    // Chord order, duplicates and slot order must not affect the ascending union.
    for (const bool reverse : {false, true}) {
        press(reverse ? 6 : 5);
        press(reverse ? 5 : 6);
        const int pitches[] = {62, 64, 65, 69};
        for (int string = 0; string < 12; ++string) {
            press(static_cast<unsigned int>(20 + string));
            if (string < 4)
                noteEvent(PerformanceEventKind::NoteOn, 2, pitches[string]);
            else
                ok &= expect(sink.events.empty(), "duplicate chord notes must not occupy extra strings");
            release(static_cast<unsigned int>(20 + string));
            if (string < 4)
                noteEvent(PerformanceEventKind::NoteOff, 2, pitches[string]);
        }
        release(6);
        press(21);
        noteEvent(PerformanceEventKind::NoteOn, 2, 69);
        release(5);
        release(21);
        noteEvent(PerformanceEventKind::NoteOff, 2, 69);
    }
    // Source transposition (including clamping) must not alter strum pitches.
    for (const int sourceTranspose : {-48, 48}) {
        ZoneWrapper::setTranspose(InstrumentType::Alpha, Zone::Zone1, sourceTranspose, pluginState.state);
        ZoneWrapper::setTranspose(InstrumentType::Alpha, Zone::Zone2, 12, pluginState.state);
        for (auto& lookup : lookups) lookup.updateAll();
        service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        for (const unsigned int sourceKey : {0u, 2u}) {
            press(sourceKey);
            press(10);
            const int expected = sourceKey == 0 ? 72 : 60;
            noteEvent(PerformanceEventKind::NoteOn, 2, expected);
            release(sourceKey);
            release(10);
            noteEvent(PerformanceEventKind::NoteOff, 2, expected);
        }
    }
    if (silentSource) {
        press(2);
        juce::MidiBuffer pending;
        service.drainPendingMidiMessages(pending);
        service.queueTransposeChangeFlush(InstrumentType::Alpha, Zone::Zone1);
        juce::MidiBuffer flushed;
        service.drainPendingMidiMessages(flushed);
        ok &= expect(flushed.isEmpty(), "flushing a silent chord must not emit Note Offs or controller resets");
        press(11);
        ok &= expect(sink.events.empty(), "a flushed source should no longer supply a strum pitch");
        release(11);
        // The destination zone must still be enabled, even for a valid silent source.
        ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone2, false, pluginState.state);
        for (auto& lookup : lookups) lookup.updateAll();
        service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
            lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
        press(2);
        press(11);
        ok &= expect(sink.events.empty(), "a disabled strum output zone must remain silent");
        release(11);
        release(2);
    }
    service.stop();
    return ok;
}

bool verifyLinkedStrumExpression(bool midi2Mode, bool chordSource, bool silentSource = false)
{
    using namespace ecm;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection lock;
    ConfigLookup lookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, lock),
        ConfigLookup(InstrumentType::Tau, pluginState, lock),
        ConfigLookup(InstrumentType::Pico, pluginState, lock)
    };
    SettingsWrapper::setMidi2Mode(midi2Mode, pluginState.state);
    SettingsWrapper::setLowerMPEVoiceCount(8, pluginState.state);
    ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone1, !silentSource, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1,
                                   silentSource ? MidiChannelType::Undefined : MidiChannelType::Chan1, pluginState.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone2, MidiChannelType::MPE_Low, pluginState.state);
    ZoneWrapper::setMidiValue(InstrumentType::Alpha, Zone::Zone2, ZoneWrapper::id_roll, {MidiValueType::CC, 20}, pluginState.state);
    ZoneWrapper::setMidiValue(InstrumentType::Alpha, Zone::Zone2, ZoneWrapper::id_yaw, {MidiValueType::CC, 21}, pluginState.state);
    ZoneWrapper::setMidiValue(InstrumentType::Alpha, Zone::Zone2, ZoneWrapper::id_pressure, {MidiValueType::CC, 22}, pluginState.state);
    auto source = makeLayoutKey(chordSource ? KeyMappingType::Chord : KeyMappingType::Note,
                                 chordSource ? "C;48;52;55;60;64;67" : "60");
    source.stringNumber = 1;
    LayoutWrapper::setLayoutKey(source, pluginState.state);
    for (int i = 1; i <= 3; ++i) {
        auto strum = makeLayoutKey(KeyMappingType::Strum,
            i == 3 ? "Strum;1;1" : "Strum;1;" + juce::String(chordSource ? i : 1) + ";0");
        strum.keyId.keyNo = i;
        strum.zone = Zone::Zone2;
        LayoutWrapper::setLayoutKey(strum, pluginState.state);
    }
    for (auto& lookup : lookups) lookup.updateAll();
    bool ok = expect(!lookups[0].keys[0][1].strumExpression && lookups[0].keys[0][3].strumExpression,
                     "saved Expression off should load, while old strum mappings default to on");
    auto saved = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, pluginState.state);
    auto restored = juce::ValueTree::fromXml(*saved.createXml());
    ok &= expect(restored.getChildWithName("key_0_1").getProperty(LayoutWrapper::id_mappingValue).toString() == "Strum;1;1;0",
                 "the expression toggle should survive layout serialization");
    MidiService service(lookups, lock);
    service.start(pluginState, nullptr);
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
    CapturingSink sink;
    osc::Message outgoing;
    juce::MidiBuffer buffer;
    uint64_t time = 1'000'000;
    auto send = [&](unsigned int key, bool active, int frames, float pressure, float roll, float yaw) {
        sink.events.clear();
        for (int i = 0; i < frames; ++i) {
            auto msg = makeKeyMessageForKey(key, pressure, roll, yaw, time += 10'000);
            msg.active = active;
            service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        }
    };
    auto noteOn = [&]() {
        for (const auto& event : sink.events)
            if (event.kind == PerformanceEventKind::NoteOn) return event;
        ok &= expect(false, "strum should start a note");
        return PerformanceEvent {};
    };
    auto checkExpression = [&](int channel, float roll, float yaw, float pressure) {
        for (int cc = 20; cc <= 22; ++cc) {
            const PerformanceEvent* found = nullptr;
            for (const auto& event : sink.events)
                if (event.kind == PerformanceEventKind::Controller && event.channel == channel && event.controller == cc)
                    found = &event;
            ok &= expect(found != nullptr, "linked source should emit all three expression controls on the strum channel");
            if (found) {
                ok &= expect(found->zoneIndex == 1, "linked expression should use the strum output zone");
                ok &= expectNear(found->value, cc == 20 ? roll : cc == 21 ? yaw : pressure, 0.0001f,
                                 "strum expression should come from the selected controller key");
            }
        }
    };
    send(0, true, 80, 1, 1, -1);
    if (silentSource)
        ok &= expect(sink.events.empty(), "a disabled source must not emit notes or expression on its own output");
    send(1, true, 6, 0.2f, -1, 1);
    const auto first = noteOn();
    checkExpression(first.channel, 1, 0, 1);
    send(2, true, 6, 0.2f, -1, 1);
    const auto second = noteOn();
    checkExpression(second.channel, 1, 0, 1);
    send(0, true, 80, 0, -1, 1);
    if (chordSource) checkExpression(first.channel, 0, 1, 0);
    checkExpression(second.channel, 0, 1, 0);
    send(1, true, 80, 1, 1, -1);
    ok &= expect(sink.events.empty(), "an Expression-off or replaced strum must not send its own expression");
    send(2, true, 80, 1, 1, -1);
    send(1, false, 1, 0, 0, 0);
    send(2, false, 1, 0, 0, 0);
    float linkedRelease = -1;
    for (const auto& event : sink.events)
        if (event.kind == PerformanceEventKind::NoteOff) linkedRelease = event.velocity;
    send(3, true, 6, 0.2f, -1, 1);
    const auto own = noteOn();
    ok &= expectNear(first.velocity, own.velocity, 0.0001f, "expression ownership must not change strike velocity");
    send(0, true, 80, 0, -1, 1);
    for (const auto& event : sink.events)
        ok &= expect(event.channel != own.channel, "source movement must not override an Expression-on strum");
    send(3, true, 80, 1, 1, -1);
    checkExpression(own.channel, 1, 0, 1);
    send(0, false, 1, 0, 0, 0);
    if (silentSource)
        ok &= expect(sink.events.empty(), "releasing a silent source must not send stray Note Offs");
    for (const auto& event : sink.events)
        ok &= expect(event.zoneIndex != 1, "releasing the source should retain the held strum's last expression and note");
    send(0, true, 80, 1, 1, -1);
    for (const auto& event : sink.events)
        ok &= expect(event.zoneIndex != 1, "re-pressing a source must not take over an older strum");
    send(3, false, 1, 0, 0, 0);
    float ownRelease = -2;
    for (const auto& event : sink.events)
        if (event.kind == PerformanceEventKind::NoteOff) ownRelease = event.velocity;
    ok &= expectNear(linkedRelease, ownRelease, 0.0001f, "expression ownership must not change release velocity");
    send(2, false, 1, 0, 0, 0);
    LayoutWrapper::setStrumSettings({0, 1, InstrumentType::Alpha}, {-1, 0, false}, pluginState.state);
    for (auto& lookup : lookups) lookup.updateAll();
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
    send(1, true, 6, 0.2f, -1, 1);
    const auto sustained = noteOn();
    send(1, false, 1, 0, 0, 0);
    send(0, true, 80, 1, 1, -1);
    checkExpression(sustained.channel, 1, 0, 1);
    send(0, false, 1, 0, 0, 0);
    bool sustainedOff = false;
    for (const auto& event : sink.events)
        if (event.kind == PerformanceEventKind::NoteOff && event.zoneIndex == 1
            && event.channel == sustained.channel) sustainedOff = true;
    ok &= expect(sustainedOff, "source release must end the sustained linked-expression voice");
    service.stop();
    return ok;
}

bool verifyRawPacketQueueCopy()
{
    juce::MidiBuffer source, destination;
    const uint32_t noteOff[] = { 0x42813e00u, 0x40000000u };
    ecm::addRawMidiEvent(source, noteOff, sizeof(noteOff), 0);
    source.addEvent(juce::MidiMessage::controllerEvent(2, 74, 0), 2);
    destination.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(64)), 3);
    destination.addEvent(juce::MidiMessage::noteOff(1, 60), 9);
    ecm::appendRawMidiBuffer(destination, source, 7);
    const int times[] = {3, 7, 9, 9};
    int index = 0;
    bool ok = expect(destination.getNumEvents() == 4, "queue copying should preserve both MIDI bytes and UMP packets");
    for (const auto metadata : destination) {
        ok &= expect(index < 4 && metadata.samplePosition == times[index], "packet copying should preserve sample offsets and ordering");
        if (index == 1)
            ok &= expect(metadata.numBytes == sizeof(noteOff)
                         && std::memcmp(metadata.data, noteOff, sizeof(noteOff)) == 0,
                         "UMP Note Off bytes must survive a queue transfer unchanged");
        ++index;
    }
    return ok;
}

bool verifyStrumReleaseAcrossZoneOutputs()
{
    using namespace ecm;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection lock;
    ConfigLookup lookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, lock),
        ConfigLookup(InstrumentType::Tau, pluginState, lock),
        ConfigLookup(InstrumentType::Pico, pluginState, lock)
    };
    SettingsWrapper::setMidi2Mode(true, pluginState.state);
    for (auto zone : { Zone::Zone2, Zone::Zone3 })
        ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, zone, MidiChannelType::Chan2, pluginState.state);
    auto chord = makeLayoutKey(KeyMappingType::Chord, "Dm;50;57;62;65;69;74");
    chord.zone = Zone::Zone2;
    LayoutWrapper::setLayoutKey(chord, pluginState.state);
    for (int string = 1; string <= 7; ++string) {
        auto strum = makeLayoutKey(KeyMappingType::Strum, "Strum;2;" + juce::String(string == 7 ? 3 : string));
        strum.keyId.keyNo = string;
        strum.zone = Zone::Zone3;
        LayoutWrapper::setLayoutKey(strum, pluginState.state);
    }
    MidiService service(lookups, lock);
    service.start(pluginState, nullptr);
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        lookups, service.getProtocol(), service.getVoiceRouter(), service.getExpressionPolicy()));
    CapturingSink sink;
    osc::Message outgoing;
    juce::MidiBuffer buffer;
    uint64_t time = 1'000'000;
    auto send = [&](unsigned int key, bool active) {
        sink.events.clear();
        for (int i = 0; i < (active ? 6 : 1); ++i) {
            auto msg = makeKeyMessageForKey(key, active ? 0.7f : 0.0f, 0, 0, time += 10'000);
            msg.active = active;
            service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        }
    };
    auto count = [&](PerformanceEventKind kind, int zone) {
        int result = 0;
        for (const auto& event : sink.events)
            if (event.kind == kind && event.zoneIndex == zone) ++result;
        return result;
    };
    bool ok = true;
    send(0, true);
    ok &= expect(count(PerformanceEventKind::NoteOn, 1) == 6, "source chord should sound on Zone 2");
    for (int pass = 0; pass < 3; ++pass) {
        for (unsigned int string = 1; string <= 6; ++string) {
            send(string, true);
            ok &= expect(count(PerformanceEventKind::NoteOn, 2) == 1, "each strum should sound on Zone 3");
            send(string, false);
            ok &= expect(count(PerformanceEventKind::NoteOff, 2) == 1,
                         "a held chord on another UMP zone must not suppress the strum's Note Off");
        }
    }
    send(0, false);
    ok &= expect(count(PerformanceEventKind::NoteOff, 1) == 6, "the source chord must release on its own zone");
    send(0, true);
    send(3, true);
    send(0, false);
    ok &= expect(count(PerformanceEventKind::NoteOff, 1) == 6, "releasing the source first must not leave its shared-pitch note hanging");
    send(3, false);
    ok &= expect(count(PerformanceEventKind::NoteOff, 2) == 1, "strum should still release after its source has released");
    send(0, true);
    send(3, true);
    send(7, true);
    ok &= expect(count(PerformanceEventKind::NoteOff, 2) == 1 && count(PerformanceEventKind::NoteOn, 2) == 1,
                 "retriggering the same output pitch should end the old voice before starting a new one");
    int noteOffIndex = -1, noteOnIndex = -1;
    for (size_t i = 0; i < sink.events.size(); ++i) {
        if (sink.events[i].kind == PerformanceEventKind::NoteOff) noteOffIndex = static_cast<int>(i);
        if (sink.events[i].kind == PerformanceEventKind::NoteOn) noteOnIndex = static_cast<int>(i);
    }
    ok &= expect(noteOffIndex >= 0 && noteOnIndex > noteOffIndex, "retrigger Note Off must precede Note On");
    send(3, false);
    ok &= expect(count(PerformanceEventKind::NoteOff, 2) == 0, "the remaining held strum should retain the retriggered voice");
    send(7, false);
    ok &= expect(count(PerformanceEventKind::NoteOff, 2) == 1, "the last held strum should end the retriggered voice");
    send(3, true);
    juce::MidiBuffer startup;
    service.drainPendingMidiMessages(startup);
    service.queueTransposeChangeFlush(InstrumentType::Alpha, Zone::Zone3);
    juce::MidiBuffer flushed;
    service.drainPendingMidiMessages(flushed);
    int flushedOffs = 0;
    for (const auto metadata : flushed) {
        if (metadata.numBytes < 8) continue;
        const auto word = juce::readUnaligned<uint32_t>(metadata.data);
        if ((word >> 28) != 4) continue;
        ok &= expect(((word >> 24) & 0xf) == 2, "strum flush messages must stay on UMP group 2 (Zone 3)");
        if (((word >> 20) & 0xf) == 8) ++flushedOffs;
    }
    ok &= expect(flushedOffs == 1, "a zone flush should release the strum despite a held source on another zone");
    send(0, false);
    ok &= expect(count(PerformanceEventKind::NoteOff, 1) == 6, "flushing strums must preserve source chord tracking");
    service.stop();
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

    for (const auto zone : { Zone::NoZone, Zone::Zone1, Zone::Zone2, Zone::Zone3 }) {
        for (const bool zoneEnabled : { true, false }) {
            for (const auto configuredZone : { Zone::Zone1, Zone::Zone2, Zone::Zone3 })
                ZoneWrapper::setEnabled(InstrumentType::Alpha, configuredZone, zoneEnabled, pluginState.state);
            SettingsWrapper::setMidi2Mode(false, pluginState.state);
            const auto legacyPanic = exerciseMapping("Trigger;AllNotesOff;0;0;0", zone);
            ok &= expect(countAllNotesOffMessages(legacyPanic) == 16,
                         "MIDI 1 All Notes Off must reach every channel regardless of the assigned or disabled zone");
            SettingsWrapper::setMidi2Mode(true, pluginState.state);
            const auto umpPanic = exerciseMapping("Trigger;AllNotesOff;0;0;0", zone);
            int channelCounts[3][16] {};
            int count = 0;
            for (const auto metadata : umpPanic) {
                if (metadata.numBytes != 8) continue;
                const auto word = juce::readUnaligned<uint32_t>(metadata.data);
                const int group = static_cast<int>((word >> 24) & 0xf);
                if ((word >> 28) == 4 && ((word >> 20) & 0xf) == 0xb
                        && ((word >> 8) & 0x7f) == 123 && group < 3) {
                    ++channelCounts[group][(word >> 16) & 0xf];
                    ++count;
                }
            }
            ok &= expect(count == 48, "MIDI 2 All Notes Off must cover all three zone groups");
            for (const auto& group : channelCounts)
                for (const auto messages : group)
                    ok &= expect(messages == 1, "each MIDI 2 zone/channel must receive All Notes Off once");
        }
    }

    return ok;
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace ecm;

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    if (argc > 1 && juce::String(argv[1]) == "--strum-routing-only") {
        const bool routingOk = verifyStrumReleaseAcrossZoneOutputs();
        juce::Logger::setCurrentLogger(nullptr);
        return routingOk ? 0 : 1;
    }
    const bool stringsOnly = argc > 1 && juce::String(argv[1]) == "--strings-only";
    const bool ok = verifyStrummingAndSixNoteChords(false, false, false, true)
                 && verifyStrummingAndSixNoteChords(true, false, false, true)
                 && verifyStrummingAndSixNoteChords(true, false, true)
                 && verifyStrummingAndSixNoteChords(false, false, true)
                 && verifyStrummingAndSixNoteChords(false, true)
                 && verifyStrummingAndSixNoteChords(true, true)
                 && verifyLinkedStrumExpression(false, true)
                 && verifyLinkedStrumExpression(true, true)
                 && verifyLinkedStrumExpression(false, false)
                 && verifyLinkedStrumExpression(false, true, true)
                 && verifyLinkedStrumExpression(true, true, true)
                 && verifyLinkedStrumExpression(false, false, true)
                 && verifyLinkedStrumExpression(true, false, true)
                 && verifyRawPacketQueueCopy()
                 && verifyStrumReleaseAcrossZoneOutputs()
                 && verifyStrummingAndSixNoteChords()
                 && verifyStrummingAndSixNoteChords(true)
                 && verifyStrummedHammerOns()
                 && verifyStrummedHammerOns(true)
                 && verifyStrummedHammerOns(false, false)
                 && verifyStrummedHammerOns(true, true, true)
                 && (stringsOnly || (verifyCenteredNoteOnAndStrideLimitedTransition()
                 && verifyMidi2KeyPitchBendScalingMatchesLegacy()
                 && verifyMpeMasterPitchBendUsesChannelMaxRange()
                 && verifySingleChannelStripPitchBendKeepsFullRange()
                 && verifyMpePitchbendEditUpdatesTransportWithoutResettingVoices()
                 && verifyMidiMessageKeysEmitMappedMessages()));
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "NoteOnExpressionTransitionTest passed" << std::endl;
    return 0;
}
