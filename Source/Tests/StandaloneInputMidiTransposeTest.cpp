#include <JuceHeader.h>

// This test intentionally exposes private members for regression checks.
#if defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#if defined(__clang__)
 #pragma clang diagnostic pop
#endif
#include "PluginProcessor.h"
#undef private

#include "Core/LayoutWrapper.h"
#include "UI/AppCtrlSectionComponent.h"
#include "Core/ExpressionCurveWrapper.h"
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

bool expectNear(double actual, double expected, double tolerance, const char* message)
{
    return expect(std::abs(actual - expected) <= tolerance, message);
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

bool verifyInitPresetDefaultState()
{
    using namespace ecm;

    ECMapperAudioProcessor processor;
    bool ok = true;
    constexpr InstrumentType deviceTypes[] = { InstrumentType::Alpha, InstrumentType::Tau, InstrumentType::Pico };
    constexpr Zone zones[] = { Zone::Zone1, Zone::Zone2, Zone::Zone3 };

    ok &= expect(processor.currentPresetSlot_.load() == 1,
                 "processor should default to preset slot 1 when no state is restored");
    ok &= expect(processor.getCurrentPresetName() == "Init",
                 "processor should default to the Init preset name when no state is restored");
    ok &= expect(processor.hasPresetSlot(1),
                 "processor should auto-create preset slot 1 when no preset bank exists yet");

    auto initSnapshot = processor.getPresetSnapshot(1);
    ok &= expect(initSnapshot.isValid(),
                 "auto-created preset slot 1 should contain a valid preset snapshot");
    if (!initSnapshot.isValid())
        return false;

    auto& liveState = processor.state.state;

    ok &= expect(SettingsWrapper::getLowerMPEVoiceCount(liveState) == 15,
                 "default Init preset should use 15 lower-zone MPE voices");
    ok &= expect(SettingsWrapper::getUpperMPEVoiceCount(liveState) == 0,
                 "default Init preset should use 0 upper-zone MPE voices");
    ok &= expect(SettingsWrapper::getLowerMPEPB(liveState) == 48,
                 "default Init preset should use 48 semitones lower-zone pitch-bend range");
    ok &= expect(SettingsWrapper::getUpperMPEPB(liveState) == 48,
                 "default Init preset should use 48 semitones upper-zone pitch-bend range");

    for (const auto deviceType : deviceTypes)
        for (const auto zone : zones)
            ok &= expect(ZoneWrapper::getTranspose(deviceType, zone, liveState) == 0,
                         "Init preset should keep every zone transpose at 0");
    ok &= expect(ZoneWrapper::getEnabled(InstrumentType::Alpha, Zone::Zone2, liveState) == false,
                 "Init preset should disable Alpha zone 2");
    ok &= expect(ZoneWrapper::getEnabled(InstrumentType::Tau, Zone::Zone3, liveState) == false,
                 "Init preset should disable Tau zone 3");
    ok &= expect(ZoneWrapper::getEnabled(InstrumentType::Pico, Zone::Zone2, liveState) == false,
                 "Init preset should disable Pico zone 2");

    const auto alphaZone1Strip = ZoneWrapper::getMidiValue(InstrumentType::Alpha,
                                                           Zone::Zone1,
                                                           ZoneWrapper::id_strip1Rel,
                                                           ZoneWrapper::default_strip1Rel,
                                                           liveState);
    ok &= expect(static_cast<int>(alphaZone1Strip.valueType) == static_cast<int>(MidiValueType::Pitchbend),
                 "Init preset should map Alpha zone 1 strip1Rel to pitch-bend");

    const auto tauRootKey = LayoutWrapper::getLayoutKey({ 0, 12, InstrumentType::Tau }, liveState);
    ok &= expect(tauRootKey.mappingValue == "60"
                 && tauRootKey.keyColour == KeyColour::Green
                 && tauRootKey.zone == Zone::Zone1
                 && tauRootKey.keyMappingType == KeyMappingType::Note,
                 "Init preset should use the requested Tau main keyboard mapping");

    const auto tauButtonKey = LayoutWrapper::getLayoutKey({ 2, 3, InstrumentType::Tau }, liveState);
    ok &= expect(tauButtonKey.mappingValue == "Trigger;AllNotesOff;0;0;0"
                 && tauButtonKey.keyColour == KeyColour::Red
                 && tauButtonKey.zone == Zone::Zone1
                 && tauButtonKey.keyMappingType == KeyMappingType::MidiMsg,
                 "Init preset should keep the Tau button All Notes Off trigger");

    const auto picoTransposeDownKey = LayoutWrapper::getLayoutKey({ 1, 0, InstrumentType::Pico }, liveState);
    ok &= expect(picoTransposeDownKey.mappingValue == "Transpose;Momentary;-12"
                 && picoTransposeDownKey.keyColour == KeyColour::Yellow
                 && picoTransposeDownKey.keyType == EigenharpKeyType::Button
                 && picoTransposeDownKey.zone == Zone::Zone1
                 && picoTransposeDownKey.keyMappingType == KeyMappingType::AppCtrl,
                 "Init preset should keep the Pico transpose-down button mapping");

    const auto picoCurve = ExpressionCurveWrapper::getCurve(InstrumentType::Pico,
                                                            ExpressionCurveTarget::Breath,
                                                            liveState).getData();
    ok &= expectNear(picoCurve.leftControl.x, 0.1588706523180008, 1.0e-6,
                     "Init preset should keep the Pico breath curve left control X value");
    ok &= expectNear(picoCurve.rightControl.y, 0.9200893044471741, 1.0e-6,
                     "Init preset should keep the Pico breath curve right control Y value");

    auto snapshotState = initSnapshot.createCopy();
    for (const auto deviceType : deviceTypes)
        for (const auto zone : zones)
            ok &= expect(ZoneWrapper::getTranspose(deviceType, zone, snapshotState) == 0,
                         "auto-created Init slot should store every zone transpose at 0");

    return ok;
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
                ok &= expect(juce::exactlyEqual(range.start, -64.0f) && juce::exactlyEqual(range.end, 63.0f),
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

bool verifyPresetKeyProgramChanges()
{
    using namespace ecm;
    struct RecordingSink : PerformanceEventSink {
        std::vector<PerformanceEvent> events;
        void pushEvent(const PerformanceEvent& event) override { events.push_back(event); }
    };
    bool ok = true;
    for (const auto* mapping : { "Preset;2", "Preset;2;0;-1;127", "Preset;2;12;45;67" }) {
        ECMapperAudioProcessor processor;
        SettingsWrapper::setMidi2Mode(false, processor.state.state);
        ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone1, MidiChannelType::Chan5, processor.state.state);
        ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone2, MidiChannelType::MPE_Low, processor.state.state);
        ZoneWrapper::setMidiChannelType(InstrumentType::Alpha, Zone::Zone3, MidiChannelType::MPE_High, processor.state.state);
        for (auto zone : { Zone::Zone1, Zone::Zone2, Zone::Zone3 })
            setZoneEnabledValue(processor, InstrumentType::Alpha, zone, true);
        LayoutWrapper::LayoutKey key;
        key.keyId = { 0, 0, InstrumentType::Alpha };
        key.keyType = EigenharpKeyType::Normal;
        key.zone = Zone::NoZone;
        key.keyMappingType = KeyMappingType::AppCtrl;
        key.mappingValue = mapping;
        LayoutWrapper::setLayoutKey(key, processor.state.state);
        processor.prepareToPlay(48000.0, 64);
        osc::Message input, output;
        input.type = osc::MessageType::Key;
        input.device = InstrumentType::Alpha;
        input.course = 0;
        input.key = 0;
        input.active = true;
        RecordingSink sink;
        juce::MidiBuffer midi;
        int preset = -1;
        processor.midiService.processMessage(input, output, midi, sink, 7, &preset);
        ok &= expect(preset == 2, "program changes should retain the preset load request");
        const auto tokens = juce::StringArray::fromTokens(mapping, ";", "");
        size_t expectedCount = 0;
        const int channels[] = { 5, 1, 16 };
        for (int zone = 0; zone < 3 && tokens.size() == 5; ++zone) {
            const int program = tokens[zone + 2].getIntValue();
            if (program < 0) continue;
            if (expectedCount < sink.events.size()) {
                const auto& event = sink.events[expectedCount];
                ok &= expect(event.kind == PerformanceEventKind::ProgramChange && event.program == program
                             && event.channel == channels[zone] && event.zoneIndex == zone && event.sampleOffset == 7,
                             "preset Program Change should preserve program, zone, channel and timing");
                processor.midiService.getProtocol()->renderEvent(midi, event);
            }
            ++expectedCount;
        }
        ok &= expect(sink.events.size() == expectedCount, "only opted-in zones should send Program Changes");
        ok &= expect(midi.getNumEvents() == static_cast<int>(expectedCount), "Program Changes should render to MIDI output");
        sink.events.clear();
        preset = -1;
        processor.midiService.processMessage(input, output, midi, sink, 8, &preset);
        input.active = false;
        processor.midiService.processMessage(input, output, midi, sink, 9, &preset);
        ok &= expect(sink.events.empty() && preset == -1, "held keys and releases should not resend preset Program Changes");
        processor.releaseResources();
    }
    return ok;
}

bool verifyLatchKeyLEDs()
{
    using namespace ecm;
    bool ok = true;
    for (const auto mode : { DeviceMode::Local, DeviceMode::ReceiveOSC }) {
        ECMapperAudioProcessor processor;
        LayoutWrapper::LayoutKey key;
        key.keyId = { 0, 0, InstrumentType::Alpha };
        key.keyType = EigenharpKeyType::Normal;
        key.keyColour = KeyColour::Off;
        key.zone = Zone::NoZone;
        key.keyMappingType = KeyMappingType::AppCtrl;
        key.mappingValue = "Transpose;Latch;12";
        LayoutWrapper::setLayoutKey(key, processor.state.state);
        processor.prepareToPlay(48000.0, 64);
        processor.hardwareService.stopService();
        ConnectedDevice device;
        device.dev = mode == DeviceMode::ReceiveOSC ? "Remote-latch-led-device@127.0.0.1" : "latch-led-device";
        if (mode == DeviceMode::ReceiveOSC) device.remoteOriginalDevId = "latch-led-device";
        device.type = InstrumentType::Alpha;
        device.mode = mode;
        device.isRemote = mode == DeviceMode::ReceiveOSC;
        if (device.isRemote) device.oscTargets.push_back({"127.0.0.1", 12130, true});
        processor.hardwareService.connectedDevices_.push_back(device);
        processor.handleAsyncUpdate();
        // Changing only the LED colour after preparing must update the runtime
        // colour used when restoring a latch, not just the physical LED.
        LayoutWrapper::setKeyColour(key.keyId, KeyColour::Red, processor.state.state);
        ok &= expect(processor.configLookups[0].keys[0][0].keyColour == KeyColour::Red,
                     "LED colour edits must update the cached key colour");
        ok &= expect(processor.runtimeConfigRefreshRequested_.load(),
                     "LED colour edits must request publication of a new audio-thread snapshot");
        processor.handleAsyncUpdate();
        osc::Message message;
        const auto drain = [&] {
            while (processor.mapperToHardwareQueue.read(message)) {}
            while (processor.outgoingOSCQueue.read(message)) {}
        };
        drain();
        osc::Message input;
        input.type = osc::MessageType::Key;
        input.device = InstrumentType::Alpha;
        std::strncpy(input.devId, "latch-led-device", 63);
        juce::MidiBuffer midi;
        int slot = -1;
        ECMapperAudioProcessor::BlockTiming timing;
        timing.numSamples = 64;
        timing.sampleRate = 48000;
        auto send = [&](bool active) {
            input.active = active;
            processor.handleHardwareMessage(input, timing, midi, slot);
        };
        auto verifyColour = [&](KeyColour expected) {
            auto& queue = mode == DeviceMode::Local ? processor.mapperToHardwareQueue : processor.outgoingOSCQueue;
            bool found = false;
            bool correct = true;
            while (queue.read(message)) {
                if (message.type == osc::MessageType::LED && message.course == 0 && message.key == 0) {
                    found = true;
                    correct &= static_cast<int>(message.value) == static_cast<int>(expected);
                }
            }
            return expect(found && correct, "every latch LED update must reach its destination with the current colour");
        };
        send(true);
        ok &= verifyColour(KeyColour::Yellow);
        send(false);
        drain();
        send(true);
        ok &= verifyColour(KeyColour::Red);
        send(false);
        if (device.isRemote) {
            processor.hardwareService.connectedDevices_.back().oscTargets[0].receiveLEDs = false;
            drain();
            send(true);
            ok &= expect(processor.outgoingOSCQueue.getMessageCount() == 0,
                         "remote latch feedback must respect disabled LED authorization");
            send(false);
        }
        processor.releaseResources();
    }
    return ok;
}

bool verifyLateHostDeviceSettingsRestore()
{
    ECMapperAudioProcessor processor;
    auto& service = processor.hardwareService;
    service.appRole_ = ecm::AppRole::Host;
    ecm::ConnectedDevice connected;
    connected.dev = "alpha-project-restore";
    connected.type = ecm::InstrumentType::Alpha;
    connected.mode = ecm::DeviceMode::Local;
    service.connectedDevices_.push_back(connected);

    auto saved = connected;
    saved.mode = ecm::DeviceMode::TransmitOSC;
    saved.oscTargets.push_back({"127.0.0.1", 12130, true});
    ecm::SettingsWrapper::saveDeviceSettings(saved, processor.state.state);
    service.restoreLocalDeviceSettings(processor.state.state);
    auto devices = service.getConnectedDevices();
    bool ok = expect(devices.front().mode == ecm::DeviceMode::TransmitOSC
                         && devices.front().oscTargets.size() == 1
                         && devices.front().oscTargets.front().port == 12130,
                     "project settings restored after hardware discovery must restore Transmit and its target");
    saved.mode = ecm::DeviceMode::Local;
    ecm::SettingsWrapper::saveDeviceSettings(saved, processor.state.state);
    service.restoreLocalDeviceSettings(processor.state.state);
    ok &= expect(service.getDeviceMode(connected.dev) == ecm::DeviceMode::Local,
                 "late project restore must also restore saved Local mode");
    return ok;
}

bool verifyToggleLEDsKeys()
{
    using namespace ecm;
    bool ok = true;
    AppCtrlSectionComponent panel;
    for (const auto* mapping : { "ToggleLEDs", "AllLEDsOff" }) {
        panel.updatePanelFromMessageString(mapping);
        ok &= expect(panel.getMessageString() == "ToggleLEDs", "new and legacy LED mappings must select Toggle LEDs");
    }
    for (int i = 0; i < panel.getNumChildComponents(); ++i) {
        if (const auto* button = dynamic_cast<const juce::ToggleButton*>(panel.getChildComponent(i)))
            if (button->getButtonText() == "Latch" || button->getButtonText() == "Momentary" || button->getButtonText() == "Trigger")
                ok &= expect(!button->isVisible(), "Toggle LEDs must have no mode controls");
    }
    for (const bool client : { false, true }) {
        ECMapperAudioProcessor processor;
        LayoutWrapper::LayoutKey key;
        key.keyId = { 0, 0, InstrumentType::Alpha };
        key.keyType = EigenharpKeyType::Normal;
        key.keyColour = KeyColour::Red;
        key.zone = Zone::Zone3;
        key.keyMappingType = KeyMappingType::AppCtrl;
        key.mappingValue = "ToggleLEDs";
        LayoutWrapper::setLayoutKey(key, processor.state.state);
        setZoneEnabledValue(processor, InstrumentType::Alpha, Zone::Zone3, false);
        processor.prepareToPlay(48000, 64);
        processor.hardwareService.stopService();
        processor.hardwareService.appRole_ = client ? AppRole::Client : AppRole::Host;
        ConnectedDevice device;
        device.dev = client ? "Remote-led-device@127.0.0.1" : "led-device";
        device.type = InstrumentType::Alpha;
        device.mode = client ? DeviceMode::ReceiveOSC : DeviceMode::Local;
        device.isRemote = client;
        if (client) {
            device.remoteOriginalDevId = "led-device";
            device.oscTargets.push_back({"127.0.0.1", 12130, true});
        }
        for (auto& course : device.assignedLEDColours)
            for (auto& colour : course) colour = static_cast<int>(KeyColour::Red);
        processor.hardwareService.connectedDevices_.push_back(device);
        osc::Message message;
        const auto drain = [&] {
            while (processor.mapperToHardwareQueue.read(message)) {}
            while (processor.outgoingOSCQueue.read(message)) {}
        };
        drain();
        const auto stateBefore = processor.state.state.createCopy();
        osc::Message input;
        input.type = osc::MessageType::Key;
        input.device = InstrumentType::Alpha;
        std::strncpy(input.devId, "led-device", 63);
        juce::MidiBuffer midi;
        int slot = -1;
        ECMapperAudioProcessor::BlockTiming timing;
        timing.numSamples = 64;
        timing.sampleRate = 48000;
        auto send = [&](bool active) {
            input.active = active;
            processor.handleHardwareMessage(input, timing, midi, slot);
        };
        send(true);
        ok &= expect(processor.mapperToHardwareQueue.getMessageCount() == 1, "Toggle LEDs must produce one command on the press edge");
        processor.hardwareService.processOutgoingMessages();
        ok &= expect(!processor.hardwareService.areLEDsEnabled(), "first press must disable LEDs");
        ok &= expect(processor.outgoingOSCQueue.read(message) && message.type == osc::MessageType::SetLEDsEnabled
                         && message.value == 0.0f && message.device == InstrumentType::None,
                     "client/host toggle must share an explicit global off request");
        ok &= expect(processor.state.state.isEquivalentTo(stateBefore), "LED off state must not change saved processor state");
        send(true);
        send(false);
        ok &= expect(processor.mapperToHardwareQueue.getMessageCount() == 0 && midi.isEmpty(),
                     "holding or releasing Toggle LEDs must not toggle or emit MIDI");
        osc::Message led;
        led.type = osc::MessageType::LED;
        led.device = InstrumentType::Alpha;
        std::strncpy(led.devId, device.dev.c_str(), 63);
        led.value = static_cast<float>(KeyColour::Yellow);
        processor.mapperToHardwareQueue.add(led);
        processor.hardwareService.processOutgoingMessages();
        ok &= expect(!processor.hardwareService.areLEDsEnabled() && processor.outgoingOSCQueue.getMessageCount() == 0,
                     "LED updates while off must not re-enable LEDs or forward client feedback");
        send(true);
        processor.hardwareService.processOutgoingMessages();
        ok &= expect(processor.hardwareService.areLEDsEnabled(), "second press must enable LEDs and refresh colours");
        ok &= expect(processor.outgoingOSCQueue.read(message) && message.type == osc::MessageType::SetLEDsEnabled && message.value == 1.0f,
                     "second client press must send the Host an explicit on request");
        if (client) {
            bool refreshed = false;
            while (processor.outgoingOSCQueue.read(message))
                if (message.type == osc::MessageType::LED && message.course == 0 && message.key == 0
                        && static_cast<int>(message.value) == static_cast<int>(KeyColour::Red)) refreshed = true;
            ok &= expect(refreshed, "enabling client LEDs must refresh the Host with the current key colours");
        } else {
            ok &= expect(processor.hardwareService.connectedDevices_[0].assignedLEDColours[0][0] == static_cast<int>(KeyColour::Red),
                         "enabling Host LEDs must restore current mapping colours instead of cached touch feedback");
        }
        send(false);
        drain();
        send(true);
        processor.hardwareService.processOutgoingMessages();
        ok &= expect(!processor.hardwareService.areLEDsEnabled(), "toggle must be reusable");
        processor.applyPresetState(processor.getPresetSnapshot(1));
        ok &= expect(processor.hardwareService.areLEDsEnabled(), "switching preset must restore LEDs");
        processor.releaseResources();
    }
    return ok;
}

bool verifyTransportKeys()
{
    using namespace ecm;
    bool ok = true;
    AppCtrlSectionComponent panel;
    for (const auto* mapping : { "Transport;Latch", "Transport;Trigger;Start", "Transport;Trigger;Stop",
                                "Preset;2", "Preset;2;0;-1;127", "Transpose;Momentary;12", "Transpose;Latch;-12" }) {
        panel.updatePanelFromMessageString(mapping);
        ok &= expect(panel.getMessageString() == mapping, "App Ctrl editor should preserve mappings");
    }
    panel.updatePanelFromMessageString("Transpose;12");
    ok &= expect(panel.getMessageString() == "Transpose;Latch;12", "legacy transpose should remain a latch");
    panel.updatePanelFromMessageString("");
    ok &= expect(panel.getMessageString() == "Preset;1", "empty mapping should reset the editor");
    for (const auto* mapping : { "Transport;Latch", "Transport;Trigger;Start", "Transport;Trigger;Stop", "Transport;Momentary;Start" }) {
        ECMapperAudioProcessor processor;
        LayoutWrapper::LayoutKey key;
        key.keyId = { 0, 0, InstrumentType::Alpha };
        key.keyType = EigenharpKeyType::Normal;
        key.keyColour = KeyColour::Off;
        key.zone = Zone::NoZone;
        key.keyMappingType = KeyMappingType::AppCtrl;
        key.mappingValue = mapping;
        LayoutWrapper::setLayoutKey(key, processor.state.state);
        processor.prepareToPlay(48000.0, 64);
        osc::Message input, output;
        input.type = osc::MessageType::Key;
        input.device = InstrumentType::Alpha;
        input.course = 0;
        input.key = 0;
        juce::MidiBuffer midi;
        auto press = [&](bool active) {
            input.active = active;
            int request = -1;
            processor.midiService.processMessage(input, output, midi, 0, nullptr, &request);
            return request;
        };
        const bool invalid = key.mappingValue.contains("Momentary");
        const int first = invalid ? -1 : key.mappingValue.endsWith("Stop") ? 0 : 1;
        ok &= expect(press(true) == first, "transport key should request its configured action");
        ok &= expect(press(true) == -1, "held transport key should not repeat");
        ok &= expect(press(false) == -1, "transport release should do nothing");
        ok &= expect(press(true) == (key.mappingValue.contains("Latch") ? 0 : first),
                     "latch should stop on second press; triggers should repeat their action");
        processor.releaseResources();
    }
    return ok;
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

bool verifyStateRestoreKeepsZoneEnabledParameters()
{
    using namespace ecm;

    ECMapperAudioProcessor sourceProcessor;
    ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone2, false, sourceProcessor.state.state);
    ZoneWrapper::setEnabled(InstrumentType::Tau, Zone::Zone3, false, sourceProcessor.state.state);
    ZoneWrapper::setEnabled(InstrumentType::Pico, Zone::Zone1, false, sourceProcessor.state.state);

    juce::MemoryBlock stateData;
    sourceProcessor.getStateInformation(stateData);

    ECMapperAudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    bool ok = true;
    ok &= expect(!ZoneWrapper::getEnabled(InstrumentType::Alpha, Zone::Zone2, restoredProcessor.state.state),
                 "restored state tree should keep Alpha zone 2 disabled");
    ok &= expect(!ZoneWrapper::getEnabled(InstrumentType::Tau, Zone::Zone3, restoredProcessor.state.state),
                 "restored state tree should keep Tau zone 3 disabled");
    ok &= expect(!ZoneWrapper::getEnabled(InstrumentType::Pico, Zone::Zone1, restoredProcessor.state.state),
                 "restored state tree should keep Pico zone 1 disabled");

    ok &= expect(!getZoneEnabledValue(restoredProcessor, InstrumentType::Alpha, Zone::Zone2),
                 "restored parameters should keep Alpha zone 2 disabled");
    ok &= expect(!getZoneEnabledValue(restoredProcessor, InstrumentType::Tau, Zone::Zone3),
                 "restored parameters should keep Tau zone 3 disabled");
    ok &= expect(!getZoneEnabledValue(restoredProcessor, InstrumentType::Pico, Zone::Zone1),
                 "restored parameters should keep Pico zone 1 disabled");

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

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (argc > 1 && juce::String(argv[1]) == "--latch-led-only")
        return verifyLatchKeyLEDs() ? 0 : 1;
    ECMapperAudioProcessor processor;

    bool ok = true;

    ok &= verifyInitPresetDefaultState();
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
    ok &= verifyPresetKeyProgramChanges();
    ok &= verifyTransportKeys();
    ok &= verifyLatchKeyLEDs();
    ok &= verifyToggleLEDsKeys();
    ok &= verifyLateHostDeviceSettingsRestore();
    ok &= verifyPresetLoadingPreservesTransportModes();
    ok &= verifyStateRestoreKeepsZoneEnabledParameters();
    ok &= verifyMappingNotesAcceptChannelsOneToFour();

    if (!ok)
        return 1;

    std::cout << "StandaloneInputMidiTransposeTest passed" << std::endl;
    return 0;
}
