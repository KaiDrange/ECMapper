#define private public
#include "UI/MainComponent.h"
#undef private

#include "PluginProcessor.h"

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

    component.midi20ModeButton.setToggleState(true, juce::dontSendNotification);
    component.midi20ModeButton.onClick();
    component.refreshFromState();

    ok &= expect(SettingsWrapper::getMidi2Mode(processor.state.state),
                 "clicking the standalone MIDI 2.0 button should enable MIDI 2.0 mode");
    ok &= expect(component.midi20ModeButton.getButtonText() == "MIDI 2.0 ON",
                 "standalone MIDI 2.0 button should show ON when MIDI 2.0 mode is active");
    ok &= expect(!component.lowerMPEVoiceCount.isEnabled() && !component.upperMPEVoiceCount.isEnabled()
                 && !component.lowerMPEPitchbendRange.isEnabled() && !component.upperMPEPitchbendRange.isEnabled(),
                 "MIDI 2.0 mode should disable legacy MPE controls");

    component.midi20ModeButton.setToggleState(false, juce::dontSendNotification);
    component.midi20ModeButton.onClick();
    component.refreshFromState();

    ok &= expect(!SettingsWrapper::getMidi2Mode(processor.state.state),
                 "clicking the standalone MIDI 2.0 button again should return to legacy MIDI mode");
    ok &= expect(component.midi20ModeButton.getButtonText() == "MIDI 2.0 OFF",
                 "standalone MIDI 2.0 button should return to OFF text after disabling MIDI 2.0 mode");

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