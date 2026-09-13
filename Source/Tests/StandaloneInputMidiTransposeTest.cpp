#define private public
#include "PluginProcessor.h"
#undef private

#include "Core/LayoutWrapper.h"
#include "Core/SettingsWrapper.h"
#include "Core/ZoneWrapper.h"

#include <cmath>
#include <iostream>
#include <utility>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

int getTransposeValue(ECMapperAudioProcessor& processor, ecm::InstrumentType deviceType, ecm::Zone zone)
{
    const auto parameterId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zone);
    if (const auto* raw = processor.state.getRawParameterValue(parameterId))
        return static_cast<int>(std::lround(raw->load()));

    return 0;
}

bool getZoneEnabledValue(ECMapperAudioProcessor& processor, ecm::InstrumentType deviceType, ecm::Zone zone)
{
    const auto parameterId = ecm::ZoneWrapper::getEnabledParameterID(deviceType, zone);
    if (const auto* raw = processor.state.getRawParameterValue(parameterId))
        return raw->load() > 0.5f;

    return false;
}

bool setZoneEnabledValue(ECMapperAudioProcessor& processor, ecm::InstrumentType deviceType, ecm::Zone zone, bool enabled)
{
    const auto parameterId = ecm::ZoneWrapper::getEnabledParameterID(deviceType, zone);
    if (auto* param = dynamic_cast<juce::AudioParameterBool*>(processor.state.getParameter(parameterId))) {
        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        return true;
    }

    return false;
}

bool applyControlMessage(ECMapperAudioProcessor& processor, int channel, int controllerNumber, int controllerValue)
{
    juce::MidiBuffer midiMessages;
    midiMessages.addEvent(juce::MidiMessage::controllerEvent(channel, controllerNumber, controllerValue), 0);
    return processor.applyZoneControlMessages(midiMessages);
}

const juce::AudioProcessorParameterGroup* findGroupByName(const juce::AudioProcessorParameterGroup& root, const juce::String& groupName)
{
    for (const auto* group : root.getSubgroups(false)) {
        if (group != nullptr && group->getName() == groupName)
            return group;
    }

    return nullptr;
}

bool verifyParameterLayoutGroupingAndRanges()
{
    using namespace ecm;

    ECMapperAudioProcessor processor;
    const auto& parameterTree = processor.getParameterTree();
    bool ok = true;

    const auto topLevelParameters = parameterTree.getParameters(false);
    ok &= expect(topLevelParameters.size() == 1,
                 "parameter tree should keep only Preset Slot at the top level");
    ok &= expect(topLevelParameters[0] == processor.state.getParameter(ECMapperAudioProcessor::presetSlotParameterId),
                 "Preset Slot should remain outside the Alpha/Tau/Pico groups");

    const auto topLevelGroups = parameterTree.getSubgroups(false);
    ok &= expect(topLevelGroups.size() == 3,
                 "parameter tree should expose exactly three top-level device groups");

    const std::pair<InstrumentType, const char*> devices[] = {
        { InstrumentType::Alpha, "Alpha" },
        { InstrumentType::Tau, "Tau" },
        { InstrumentType::Pico, "Pico" }
    };

    for (const auto& [deviceType, deviceName] : devices)
    {
        const auto* group = findGroupByName(parameterTree, deviceName);
        ok &= expect(group != nullptr,
                     (juce::String("parameter tree should contain the ") + deviceName + " group").toRawUTF8());
        if (group == nullptr)
            continue;

        ok &= expect(group->getParent() == &parameterTree,
                     (juce::String(deviceName) + " should be a top-level parameter group").toRawUTF8());

        const auto groupedParameters = group->getParameters(false);
        ok &= expect(groupedParameters.size() == 6,
                     (juce::String(deviceName) + " should contain 6 zone parameters").toRawUTF8());

        for (int zone = static_cast<int>(Zone::Zone1); zone <= static_cast<int>(Zone::Zone3); ++zone)
        {
            const auto zoneType = static_cast<Zone>(zone);
            const auto zoneLabel = juce::String("Zone ") + juce::String(zone);

            const auto enabledId = ZoneWrapper::getEnabledParameterID(deviceType, zoneType);
            auto* enabledParameter = processor.state.getParameter(enabledId);
            ok &= expect(enabledParameter != nullptr,
                         (juce::String(deviceName) + " " + zoneLabel + " should keep its enable parameter ID").toRawUTF8());
            ok &= expect(parameterTree.getGroupsForParameter(enabledParameter).contains(group),
                         (juce::String(deviceName) + " " + zoneLabel + " enable should belong to the device group").toRawUTF8());
            if (enabledParameter != nullptr)
                ok &= expect(enabledParameter->getName(128) == zoneLabel + " Enable",
                             (juce::String(deviceName) + " " + zoneLabel + " enable should use a host-friendly name").toRawUTF8());

            const auto transposeId = ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
            auto* transposeBaseParameter = processor.state.getParameter(transposeId);
            ok &= expect(transposeBaseParameter != nullptr,
                         (juce::String(deviceName) + " " + zoneLabel + " should keep its transpose parameter ID").toRawUTF8());
            ok &= expect(parameterTree.getGroupsForParameter(transposeBaseParameter).contains(group),
                         (juce::String(deviceName) + " " + zoneLabel + " transpose should belong to the device group").toRawUTF8());
            if (transposeBaseParameter != nullptr)
                ok &= expect(transposeBaseParameter->getName(128) == zoneLabel + " Transpose",
                             (juce::String(deviceName) + " " + zoneLabel + " transpose should use a host-friendly name").toRawUTF8());

            auto* transposeParameter = dynamic_cast<juce::AudioParameterInt*>(transposeBaseParameter);
            ok &= expect(transposeParameter != nullptr,
                         (juce::String(deviceName) + " " + zoneLabel + " transpose should remain an integer parameter").toRawUTF8());
            if (transposeParameter != nullptr)
            {
                const auto range = transposeParameter->getNormalisableRange();
                ok &= expect(range.start == -64.0f && range.end == 63.0f,
                             (juce::String(deviceName) + " " + zoneLabel + " transpose should match the MIDI CC range").toRawUTF8());
            }
        }
    }

    return ok;
}

bool verifyPluginDirectMidiMessageKeyRouting()
{
    using namespace ecm;

    ECMapperAudioProcessor processor;
    SettingsWrapper::setPluginOutputMode(OutputTransportMode::Vst3Direct, processor.state.state);
    SettingsWrapper::setMidi2Mode(false, processor.state.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan1, processor.state.state);

    LayoutWrapper::LayoutKey layoutKey;
    layoutKey.keyId = { 0, 0, InstrumentType::Alpha };
    layoutKey.keyType = EigenharpKeyType::Normal;
    layoutKey.keyColour = KeyColour::Off;
    layoutKey.zone = Zone::Zone1;
    layoutKey.keyMappingType = KeyMappingType::MidiMsg;
    layoutKey.mappingValue = "Trigger;CC;74;0;127";
    LayoutWrapper::setLayoutKey(layoutKey, processor.state.state);

    processor.prepareToPlay(48000.0, 64);

    osc::Message message;
    message.type = osc::MessageType::Key;
    message.device = InstrumentType::Alpha;
    message.course = 0;
    message.key = 0;
    message.active = true;
    message.timestamp = 1'000'000ULL;
    std::strncpy(message.devId, "test-device", 63);
    processor.hardwareToMapperQueue.add(message);

    juce::AudioBuffer<float> audioBuffer(2, 64);
    juce::MidiBuffer midiMessages;
    processor.processBlock(audioBuffer, midiMessages);
    processor.releaseResources();

    const auto directEvents = processor.drainPendingVst3DirectEvents();
    for (const auto& event : directEvents)
    {
        if (event.kind == Vst3DirectEventKind::LegacyCC
            && event.channel == 0
            && event.controller == 74
            && std::abs(event.value - 1.0) < 1.0e-6)
            return true;
    }

    return expect(false, "plugin direct mode should forward MIDI message command keys as legacy controller events");
}

bool verifyPluginDirectMidiMessageKeyRoutingWithoutZone()
{
    using namespace ecm;

    ECMapperAudioProcessor processor;
    SettingsWrapper::setPluginOutputMode(OutputTransportMode::Vst3Direct, processor.state.state);
    SettingsWrapper::setMidi2Mode(false, processor.state.state);
    ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan1, processor.state.state);

    LayoutWrapper::LayoutKey layoutKey;
    layoutKey.keyId = { 0, 0, InstrumentType::Alpha };
    layoutKey.keyType = EigenharpKeyType::Normal;
    layoutKey.keyColour = KeyColour::Off;
    layoutKey.zone = Zone::NoZone;
    layoutKey.keyMappingType = KeyMappingType::MidiMsg;
    layoutKey.mappingValue = "Trigger;CC;74;0;127";
    LayoutWrapper::setLayoutKey(layoutKey, processor.state.state);

    processor.prepareToPlay(48000.0, 64);

    osc::Message message;
    message.type = osc::MessageType::Key;
    message.device = InstrumentType::Alpha;
    message.course = 0;
    message.key = 0;
    message.active = true;
    message.timestamp = 1'000'000ULL;
    std::strncpy(message.devId, "test-device", 63);
    processor.hardwareToMapperQueue.add(message);

    juce::AudioBuffer<float> audioBuffer(2, 64);
    juce::MidiBuffer midiMessages;
    processor.processBlock(audioBuffer, midiMessages);
    processor.releaseResources();

    const auto directEvents = processor.drainPendingVst3DirectEvents();
    for (const auto& event : directEvents)
    {
        if (event.kind == Vst3DirectEventKind::LegacyCC
            && event.channel == 0
            && event.controller == 74
            && std::abs(event.value - 1.0) < 1.0e-6)
            return true;
    }

    return expect(false, "plugin direct mode should still forward MIDI message command keys when the key has no explicit zone");
}

bool verifyPresetLoadingPreservesTransportModes()
{
    using namespace ecm;

    ECMapperAudioProcessor processor;

    SettingsWrapper::setMidi2Mode(false, processor.state.state);
    SettingsWrapper::setPluginOutputMode(OutputTransportMode::LegacyMidi, processor.state.state);
    ZoneWrapper::setTranspose(InstrumentType::Alpha, Zone::Zone1, 5, processor.state.state);
    if (!processor.savePresetSlot(2, "Legacy Transport Test"))
        return expect(false, "saving the legacy transport preset should succeed");

    SettingsWrapper::setMidi2Mode(true, processor.state.state);
    SettingsWrapper::setPluginOutputMode(OutputTransportMode::Vst3Direct, processor.state.state);
    ZoneWrapper::setTranspose(InstrumentType::Alpha, Zone::Zone1, 9, processor.state.state);
    if (!processor.savePresetSlot(3, "Direct Transport Test"))
        return expect(false, "saving the direct transport preset should succeed");

    SettingsWrapper::setMidi2Mode(true, processor.state.state);
    SettingsWrapper::setPluginOutputMode(OutputTransportMode::Vst3Direct, processor.state.state);
    ZoneWrapper::setTranspose(InstrumentType::Alpha, Zone::Zone1, 1, processor.state.state);
    if (!processor.loadPresetSlot(2))
        return expect(false, "loading preset slot 2 should succeed");

    bool ok = true;
    ok &= expect(ZoneWrapper::getTranspose(InstrumentType::Alpha, Zone::Zone1, processor.state.state) == 5,
                 "loading preset slot 2 should still restore preset-controlled transpose");
    ok &= expect(SettingsWrapper::getMidi2Mode(processor.state.state),
                 "loading another preset should not switch the current MIDI 2.0 mode");
    ok &= expect(SettingsWrapper::getPluginOutputMode(processor.state.state) == OutputTransportMode::Vst3Direct,
                 "loading another preset should not switch the current plugin direct/legacy mode");

    SettingsWrapper::setMidi2Mode(false, processor.state.state);
    SettingsWrapper::setPluginOutputMode(OutputTransportMode::LegacyMidi, processor.state.state);
    ZoneWrapper::setTranspose(InstrumentType::Alpha, Zone::Zone1, -2, processor.state.state);
    if (!processor.loadPresetSlot(3))
        return expect(false, "loading preset slot 3 should succeed");

    ok &= expect(ZoneWrapper::getTranspose(InstrumentType::Alpha, Zone::Zone1, processor.state.state) == 9,
                 "loading preset slot 3 should still restore preset-controlled transpose");
    ok &= expect(!SettingsWrapper::getMidi2Mode(processor.state.state),
                 "loading a preset should not force MIDI 2.0 mode on when it is currently off");
    ok &= expect(SettingsWrapper::getPluginOutputMode(processor.state.state) == OutputTransportMode::LegacyMidi,
                 "loading a preset should not force plugin direct mode on when legacy mode is currently selected");

    return ok;
}

bool verifyMappingNotesAcceptChannelsOneToFour()
{
    ECMapperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);

    juce::MidiBuffer midiMessages;
    midiMessages.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(100)), 0);
    midiMessages.addEvent(juce::MidiMessage::noteOn(2, 61, juce::uint8(100)), 1);
    midiMessages.addEvent(juce::MidiMessage::noteOn(3, 62, juce::uint8(100)), 2);
    midiMessages.addEvent(juce::MidiMessage::noteOn(4, 63, juce::uint8(100)), 3);

    juce::AudioBuffer<float> audioBuffer(2, 64);
    processor.processBlock(audioBuffer, midiMessages);

    std::vector<juce::MidiMessage> selectionMessages;
    processor.drainKeyboardSelectionMessages(selectionMessages);
    processor.releaseResources();

    bool ok = true;
    ok &= expect(selectionMessages.size() == 4,
                 "mapping note input should accept note messages from MIDI channels 1-4");

    for (size_t index = 0; index < selectionMessages.size(); ++index)
    {
        const auto& message = selectionMessages[index];
        ok &= expect(message.isNoteOn(),
                     "mapping note input should queue note-on messages for selection");
        ok &= expect(message.getChannel() == static_cast<int>(index) + 1,
                     "mapping note input should preserve the original MIDI channel");
        ok &= expect(message.getNoteNumber() == 60 + static_cast<int>(index),
                     "mapping note input should preserve the original note number");
    }

    return ok;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    ECMapperAudioProcessor processor;

    bool ok = true;

    ok &= verifyParameterLayoutGroupingAndRanges();

    ok &= expect(applyControlMessage(processor, 1, 22, 0),
                 "global transpose CC should report a change when it updates zone 1 on all devices");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == -64
                 && getTransposeValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone1) == -64
                 && getTransposeValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone1) == -64,
                 "CC 22 on channel 1 should set zone 1 transpose for Alpha, Tau, and Pico");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == 0
                 && getTransposeValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone3) == 0,
                 "global zone 1 transpose should not affect other zones");

    ok &= expect(applyControlMessage(processor, 3, 23, 63),
                 "device-specific transpose CC should report a change when it updates Tau zone 2");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone2) == -1,
                 "CC 23 on channel 3 should map to Tau zone 2 transpose");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == 0
                 && getTransposeValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone2) == 0,
                 "device-specific transpose should not affect other devices");

    ok &= expect(applyControlMessage(processor, 4, 24, 65),
                 "device-specific transpose CC should report a change when it updates Pico zone 3");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone3) == 1,
                 "CC 24 on channel 4 should map to Pico zone 3 transpose");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone3) == 0
                 && getTransposeValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone3) == 0,
                 "device-specific zone 3 transpose should leave other devices unchanged");

    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1, true),
                 "test setup should be able to enable Alpha zone 1");
    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone1, true),
                 "test setup should be able to enable Tau zone 1");
    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone1, true),
                 "test setup should be able to enable Pico zone 1");
    ok &= expect(applyControlMessage(processor, 1, 25, 0),
                 "global zone enable CC should report a change when it disables zone 1 on all devices");
    ok &= expect(!getZoneEnabledValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1)
                 && !getZoneEnabledValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone1)
                 && !getZoneEnabledValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone1),
                 "CC 25 on channel 1 should disable zone 1 for Alpha, Tau, and Pico");

    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2, false),
                 "test setup should be able to disable Alpha zone 2");
    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone2, false),
                 "test setup should be able to disable Tau zone 2");
    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone2, false),
                 "test setup should be able to disable Pico zone 2");
    ok &= expect(applyControlMessage(processor, 2, 26, 127),
                 "device-specific zone enable CC should report a change when it enables Alpha zone 2");
    ok &= expect(getZoneEnabledValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2),
                 "CC 26 on channel 2 should enable Alpha zone 2");
    ok &= expect(!getZoneEnabledValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone2)
                 && !getZoneEnabledValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone2),
                 "device-specific zone enable should not affect other devices");

    ok &= expect(setZoneEnabledValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone3, false),
                 "test setup should be able to disable Pico zone 3");
    ok &= expect(applyControlMessage(processor, 4, 27, 64),
                 "device-specific zone enable CC should report a change when it enables Pico zone 3");
    ok &= expect(getZoneEnabledValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone3),
                 "CC 27 on channel 4 should enable Pico zone 3");

    ok &= verifyPluginDirectMidiMessageKeyRouting();
    ok &= verifyPluginDirectMidiMessageKeyRoutingWithoutZone();
    ok &= verifyPresetLoadingPreservesTransportModes();
    ok &= verifyMappingNotesAcceptChannelsOneToFour();

    if (!ok)
        return 1;

    std::cout << "StandaloneInputMidiTransposeTest passed" << std::endl;
    return 0;
}