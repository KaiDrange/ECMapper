#define private public
#include "UI/MainComponent.h"
#include "PluginProcessor.h"
#undef private

#include <iostream>

namespace {

bool midiBufferIsEmpty(const juce::MidiBuffer& buffer)
{
    for (const auto _ : buffer)
        juce::ignoreUnused(_);

    return buffer.isEmpty();
}

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    using ecm::SettingsWrapper;

    ECMapperAudioProcessor processor;
    SettingsWrapper::setMidi2Mode(false, processor.state.state);
    SettingsWrapper::setPluginOutputMode(ecm::OutputTransportMode::LegacyMidi, processor.state.state);

    ecm::MainComponent component(processor.state, processor.getHardwareService(), processor, nullptr);
    component.isStandaloneApp_ = true;
    component.refreshFromState();
    component.resized();

    bool ok = true;
    ok &= expect(component.calibrationButton.getButtonText() == juce::String(juce::CharPointer_UTF8("\xE2\x9A\x99")),
                 "main component should show a cogwheel calibration button");
    ok &= expect(component.calibrationButton.getTooltip().containsIgnoreCase("calibration"),
                 "calibration button should explain that it opens calibration settings");
    ok &= expect(component.calibrationButton.getX() >= component.picoTabButton.getRight(),
                 "calibration button should be positioned to the right of the tab buttons");
    ok &= expect(component.calibrationButton.getWidth() <= component.communicationTabButton.getWidth(),
                 "calibration button should remain compact compared with the main tab buttons");

    ok &= expect(component.midi20ModeButton.getButtonText() == "MIDI 2.0 OFF",
                 "standalone MIDI 2.0 button should show OFF when legacy MIDI mode is active");
    ok &= expect(component.midi20ModeButton.getTooltip().containsIgnoreCase("legacy MIDI")
                 && component.midi20ModeButton.getTooltip().containsIgnoreCase("choose MIDI 2.0")
                 && component.midi20ModeButton.getTooltip().containsIgnoreCase("next time ECMapper is started"),
                 "standalone MIDI 2.0 button should explain legacy fallback and restart behavior");
    ok &= expect(component.lowerMPEVoiceCount.isEnabled() && component.upperMPEVoiceCount.isEnabled()
                 && component.lowerMPEPitchbendRange.isEnabled() && component.upperMPEPitchbendRange.isEnabled(),
                 "legacy MIDI mode should keep MPE controls enabled");
    ok &= expect(!processor.getMidiService().isVirtualOutputActive(),
                 "legacy standalone mode should not create a Direct virtual MIDI output");

    for (const auto& virtualEndpoint : processor.getMidiService().virtualEndpoints_)
        ok &= expect(!virtualEndpoint.isAlive(),
                     "legacy standalone mode should not advertise Direct UMP endpoints");

    component.midi20ModeButton.setToggleState(true, juce::dontSendNotification);
    component.midi20ModeButton.onClick();
    component.refreshFromState();

    ok &= expect(SettingsWrapper::getMidi2Mode(processor.state.state),
                 "clicking the standalone MIDI 2.0 button should enable MIDI 2.0 mode");
    ok &= expect(component.midi20ModeButton.getButtonText() == "MIDI 2.0 ON",
                 "standalone MIDI 2.0 button should show ON when MIDI 2.0 mode is active");
    ok &= expect(component.lowerMPEVoiceCount.isEnabled() && component.upperMPEVoiceCount.isEnabled()
                 && component.lowerMPEPitchbendRange.isEnabled() && component.upperMPEPitchbendRange.isEnabled(),
                 "MIDI 2.0 mode should keep MPE controls enabled when channel layout is still used");

    juce::ValueTree legacyDevice(ecm::LayoutWrapper::id_device + juce::String((int)ecm::InstrumentType::Alpha));
    auto legacyLayout = legacyDevice.getOrCreateChildWithName(ecm::LayoutWrapper::id_layout, nullptr);
    auto legacyKey = legacyLayout.getOrCreateChildWithName(ecm::LayoutWrapper::id_key + juce::String("_0_4"), nullptr);
    legacyKey.setProperty(ecm::LayoutWrapper::id_keyType, (int)ecm::EigenharpKeyType::Normal, nullptr);
    legacyKey.setProperty(ecm::LayoutWrapper::id_keyColour, (int)ecm::KeyColour::Off, nullptr);
    legacyKey.setProperty(ecm::LayoutWrapper::id_zone, (int)ecm::Zone::Zone1, nullptr);
    legacyKey.setProperty(ecm::LayoutWrapper::id_keyMappingType, (int)ecm::KeyMappingType::Note, nullptr);
    legacyKey.setProperty(ecm::LayoutWrapper::id_mappingValue, "72", nullptr);
    processor.state.state.addChild(legacyDevice, -1, nullptr);

    component.selectTab(1);
    component.refreshFromState();
    ok &= expect(ecm::LayoutWrapper::getLayoutKey({ 0, 4, ecm::InstrumentType::Alpha }, processor.state.state).mappingValue == "72",
                 "a legacy alpha layout should remain readable before editing the lower MPE pitch-bend range");

    juce::MidiBuffer startupMessages;
    processor.getMidiService().drainPendingMidiMessages(startupMessages, 0);

    component.lowerMPEPitchbendRange.setValue(11);
    component.lowerMPEPitchbendRange.input.onFocusLost();
    component.refreshFromState();

    juce::MidiBuffer pendingMessagesAfterPbEdit;
    processor.getMidiService().drainPendingMidiMessages(pendingMessagesAfterPbEdit, 0);

    ok &= expect(ecm::SettingsWrapper::getLowerMPEPB(processor.state.state) == 11,
                 "editing the lower MPE pitch-bend range through the main component should update the stored setting");
    ok &= expect(ecm::LayoutWrapper::getLayoutKey({ 0, 4, ecm::InstrumentType::Alpha }, processor.state.state).mappingValue == "72",
                 "editing the lower MPE pitch-bend range should not clear a migrated alpha layout");
    ok &= expect(!processor.state.state.getChildWithName(ecm::LayoutWrapper::id_device + juce::String((int)ecm::InstrumentType::Alpha)).isValid(),
                 "editing the lower MPE pitch-bend range should normalize legacy root device data out of the root state");
    ok &= expect(ecm::SettingsWrapper::getPresetTree(processor.state.state)
                     .getChildWithName(ecm::LayoutWrapper::id_device + juce::String((int)ecm::InstrumentType::Alpha))
                     .isValid(),
                 "editing the lower MPE pitch-bend range should keep the alpha device tree inside the preset subtree");
    ok &= expect(midiBufferIsEmpty(pendingMessagesAfterPbEdit),
                 "editing the lower MPE pitch-bend range through the UI should not queue a live legacy transport layout reset");

    auto loadedPicoKey = ecm::LayoutWrapper::KeyId { 0, 3, ecm::InstrumentType::Pico };
    auto loadedPicoLayout = ecm::SettingsWrapper::getPresetTree(processor.state.state)
        .getOrCreateChildWithName(ecm::LayoutWrapper::id_device + juce::String((int)ecm::InstrumentType::Pico), nullptr)
        .getOrCreateChildWithName(ecm::LayoutWrapper::id_layout, nullptr);
    auto presetPicoKey = loadedPicoLayout.getOrCreateChildWithName(ecm::LayoutWrapper::id_key + juce::String("_0_3"), nullptr);
    presetPicoKey.setProperty(ecm::LayoutWrapper::id_keyType, (int)ecm::EigenharpKeyType::Normal, nullptr);
    presetPicoKey.setProperty(ecm::LayoutWrapper::id_keyColour, (int)ecm::KeyColour::Off, nullptr);
    presetPicoKey.setProperty(ecm::LayoutWrapper::id_zone, (int)ecm::Zone::Zone1, nullptr);
    presetPicoKey.setProperty(ecm::LayoutWrapper::id_keyMappingType, (int)ecm::KeyMappingType::Note, nullptr);
    presetPicoKey.setProperty(ecm::LayoutWrapper::id_mappingValue, "67", nullptr);

    juce::ValueTree stalePicoDevice(ecm::LayoutWrapper::id_device + juce::String((int)ecm::InstrumentType::Pico));
    auto stalePicoLayout = stalePicoDevice.getOrCreateChildWithName(ecm::LayoutWrapper::id_layout, nullptr);
    auto stalePicoKey = stalePicoLayout.getOrCreateChildWithName(ecm::LayoutWrapper::id_key + juce::String("_0_3"), nullptr);
    stalePicoKey.setProperty(ecm::LayoutWrapper::id_keyType, (int)ecm::EigenharpKeyType::Normal, nullptr);
    stalePicoKey.setProperty(ecm::LayoutWrapper::id_keyColour, (int)ecm::KeyColour::Off, nullptr);
    stalePicoKey.setProperty(ecm::LayoutWrapper::id_zone, (int)ecm::Zone::Zone1, nullptr);
    stalePicoKey.setProperty(ecm::LayoutWrapper::id_keyMappingType, (int)ecm::KeyMappingType::Note, nullptr);
    stalePicoKey.setProperty(ecm::LayoutWrapper::id_mappingValue, "0", nullptr);
    processor.state.state.addChild(stalePicoDevice, -1, nullptr);

    component.selectTab(3);
    component.refreshFromState();
    component.lowerMPEPitchbendRange.setValue(13);
    component.lowerMPEPitchbendRange.input.onFocusLost();
    component.refreshFromState();

    ok &= expect(ecm::LayoutWrapper::getLayoutKey(loadedPicoKey, processor.state.state).mappingValue == "67",
                 "editing the lower MPE pitch-bend range should preserve an already-loaded pico layout when duplicate legacy root data exists");
    ok &= expect(!processor.state.state.getChildWithName(ecm::LayoutWrapper::id_device + juce::String((int)ecm::InstrumentType::Pico)).isValid(),
                 "editing the lower MPE pitch-bend range should still remove stale pico root device data after migration");

    processor.getMidiService().stop();
    processor.getMidiService().start(processor.state, &processor.getHardwareService());
    ok &= expect(processor.getMidiService().isVirtualOutputActive(),
                 "starting standalone MIDI 2.0 mode should create the Direct UMP output");

    for (size_t zoneIndex = 0; zoneIndex < processor.getMidiService().virtualEndpoints_.size(); ++zoneIndex)
    {
        const auto directEndpoint = juce::universal_midi_packets::Endpoints::getInstance()->getEndpoint(
            processor.getMidiService().virtualEndpoints_[zoneIndex].getId());
        ok &= expect(directEndpoint.has_value(),
                     "starting standalone MIDI 2.0 mode should advertise all three Direct UMP endpoints");
        if (directEndpoint.has_value())
        {
            const auto blocks = directEndpoint->getBlocks();
            ok &= expect(blocks.size() == 1,
                         "each standalone MIDI 2.0 Direct UMP port should expose one zone block");
        }
    }

    for (size_t zoneIndex = 0; zoneIndex < processor.getMidiService().virtualEndpoints_.size(); ++zoneIndex)
    {
        const auto directEndpoint = juce::universal_midi_packets::Endpoints::getInstance()->getEndpoint(
            processor.getMidiService().virtualEndpoints_[zoneIndex].getId());
        ok &= expect(directEndpoint.has_value(),
                     "standalone MIDI 2.0 mode should advertise all three Direct UMP endpoints");
        if (directEndpoint.has_value())
            ok &= expect(directEndpoint->getName() == "ECMapper Direct UMP Zone " + juce::String(static_cast<int>(zoneIndex) + 1),
                         "standalone MIDI 2.0 mode should advertise correctly named Direct UMP endpoints");
    }

    processor.setMidiOutput(nullptr);
    ok &= expect(processor.getMidiService().isVirtualTarget_,
                 "standalone MIDI 2.0 mode should keep using the internal Direct UMP output when no external output is selected");
    for (const auto& directUmpOutput : processor.getMidiService().directUmpOutputs_)
        ok &= expect(directUmpOutput.isAlive(),
                     "standalone MIDI 2.0 mode should keep each internal Direct UMP connection alive when no external output is selected");

    component.midi20ModeButton.setToggleState(false, juce::dontSendNotification);
    component.midi20ModeButton.onClick();
    component.refreshFromState();

    ok &= expect(!SettingsWrapper::getMidi2Mode(processor.state.state),
                 "clicking the standalone MIDI 2.0 button again should return to legacy MIDI mode");
    ok &= expect(component.midi20ModeButton.getButtonText() == "MIDI 2.0 OFF",
                 "standalone MIDI 2.0 button should return to OFF text after disabling MIDI 2.0 mode");

    processor.getMidiService().stop();
    processor.getMidiService().start(processor.state, &processor.getHardwareService());
    ok &= expect(!processor.getMidiService().isVirtualOutputActive(),
                 "returning to legacy MIDI mode should remove the Direct virtual UMP outputs");

    component.isStandaloneApp_ = false;
    component.refreshFromState();
    component.resized();

    ok &= expect(component.vst3DirectModeButton.getButtonText() == "VST3 Direct OFF",
                 "plugin VST3 Direct button should show OFF when legacy MIDI mode is active");
    ok &= expect(component.vst3DirectModeButton.getTooltip().containsIgnoreCase("legacy MIDI")
                 && component.vst3DirectModeButton.getTooltip().containsIgnoreCase("choose VST3 Direct")
                 && component.vst3DirectModeButton.getTooltip().containsIgnoreCase("next time ECMapper is started"),
                 "plugin VST3 Direct button should explain legacy fallback and when to choose it");

    component.vst3DirectModeButton.setToggleState(true, juce::dontSendNotification);
    component.vst3DirectModeButton.onClick();
    component.refreshFromState();

    ok &= expect(SettingsWrapper::getPluginOutputMode(processor.state.state) == ecm::OutputTransportMode::Vst3Direct,
                 "clicking the plugin VST3 Direct button should enable VST3 Direct mode");
    ok &= expect(component.vst3DirectModeButton.getButtonText() == "VST3 Direct ON",
                 "plugin VST3 Direct button should show ON when VST3 Direct mode is active");

    component.vst3DirectModeButton.setToggleState(false, juce::dontSendNotification);
    component.vst3DirectModeButton.onClick();
    component.refreshFromState();

    ok &= expect(SettingsWrapper::getPluginOutputMode(processor.state.state) == ecm::OutputTransportMode::LegacyMidi,
                 "clicking the plugin VST3 Direct button again should return to legacy MIDI mode");
    ok &= expect(component.vst3DirectModeButton.getButtonText() == "VST3 Direct OFF",
                 "plugin VST3 Direct button should return to OFF text after disabling VST3 Direct mode");

    if (!ok)
        return 1;

    std::cout << "MainComponentMidi2ToggleTest passed" << std::endl;
    return 0;
}