#define private public
#include "PluginProcessor.h"
#undef private

#include "Core/LayoutWrapper.h"
#include "Core/SettingsWrapper.h"
#include "Core/ZoneWrapper.h"

#include <cmath>
#include <iostream>

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

bool applyTransposeMessage(ECMapperAudioProcessor& processor, int channel, int ccValue)
{
    juce::MidiBuffer midiMessages;
    midiMessages.addEvent(juce::MidiMessage::controllerEvent(channel, 22, ccValue), 0);
    return processor.applyZoneControlMessages(midiMessages);
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

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    ECMapperAudioProcessor processor;

    bool ok = true;

    ok &= expect(applyTransposeMessage(processor, 1, 0),
                 "transpose CC should report a change when it updates zone 1");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == -64,
                 "CC 22 value 0 should map to -64 semitones");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == 0,
                 "channel 1 transpose CC should not affect zone 2");

    ok &= expect(applyTransposeMessage(processor, 2, 63),
                 "transpose CC should report a change when it updates zone 2");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == -1,
                 "CC 22 value 63 should map to -1 semitone");

    ok &= expect(!applyTransposeMessage(processor, 3, 64),
                 "transpose CC should not report a change when value 64 keeps zone 3 at 0 semitones");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone3) == 0,
                 "CC 22 value 64 should map to 0 semitones");

    ok &= expect(applyTransposeMessage(processor, 4, 65),
                 "channel 4 transpose CC should report a change when it updates all zones");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == 1
                 && getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == 1
                 && getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone3) == 1,
                 "CC 22 value 65 on channel 4 should map to +1 semitone for all zones");

    ok &= expect(applyTransposeMessage(processor, 4, 127),
                 "channel 4 transpose CC should report a change at the top of the range");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == 63
                 && getTransposeValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone2) == 63
                 && getTransposeValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone3) == 63,
                 "CC 22 value 127 should map to +63 semitones across all devices and zones");

    ok &= verifyPluginDirectMidiMessageKeyRouting();
    ok &= verifyPluginDirectMidiMessageKeyRoutingWithoutZone();
    ok &= verifyPresetLoadingPreservesTransportModes();

    if (!ok)
        return 1;

    std::cout << "StandaloneInputMidiTransposeTest passed" << std::endl;
    return 0;
}