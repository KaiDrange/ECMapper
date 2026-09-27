#include <iostream>

#include "PluginProcessor.h"
#include "Core/SettingsWrapper.h"
#include "UI/CorePage.h"

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

struct TestPlayHead : juce::AudioPlayHead {
    PositionInfo position;
    juce::Optional<PositionInfo> getPosition() const override { return position; }
};

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    ECMapperAudioProcessor processor;

    bool ok = true;
    ok &= expect(!ecm::SettingsWrapper::getMidi2Mode(processor.state.state),
                 "fresh plugin state should keep MIDI 2.0 mode disabled by default");
    ok &= expect(ecm::SettingsWrapper::getPluginOutputMode(processor.state.state) == ecm::OutputTransportMode::Vst3Direct,
                 "fresh plugin state should default to VST3 Direct mode");

    ecm::CorePage page(processor.getHardwareService(), processor.state.state);
    juce::ToggleButton* hostButton = nullptr;
    for (auto* child : page.getChildren()) {
        if (auto* button = dynamic_cast<juce::ToggleButton*>(child)) {
            const auto text = button->getButtonText();
            if (text == "MIDI Clock In" || text == "MIDI Clock Master")
                ok &= expect(!button->isVisible(), "plugins must hide MIDI clock input and output choices");
            if (text == "Sync to Host") hostButton = button;
            if (text == "Ableton Link")
                ok &= expect(button->isVisible(), "plugins must retain Ableton Link");
        }
    }
    ok &= expect(hostButton != nullptr && hostButton->isVisible(), "plugins must offer Sync to Host");
    if (hostButton != nullptr) hostButton->onClick();
    ok &= expect(ecm::SettingsWrapper::getClockSettings(processor.state.state)
                     .getProperty(ecm::SettingsWrapper::id_clockSource).toString() == "host",
                 "the host-sync control must select the processor's host clock");

    auto audioSettings = ecm::SettingsWrapper::getAudioOutputSettings(processor.state.state);
    audioSettings.setProperty(ecm::SettingsWrapper::id_metronomeRoute, 2, nullptr);
    TestPlayHead playhead;
    playhead.position.setBpm(137.0);
    playhead.position.setTimeSignature(juce::AudioPlayHead::TimeSignature{7, 8});
    playhead.position.setPpqPosition(0.0);
    playhead.position.setPpqPositionOfLastBarStart(0.0);
    playhead.position.setIsPlaying(true);
    processor.setPlayHead(&playhead);
    processor.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> audio(juce::jmax(2, processor.getTotalNumOutputChannels()), 128);
    juce::MidiBuffer midi;
    audio.clear();
    processor.processBlock(audio, midi);
    auto& service = processor.getHardwareService();
    ok &= expect(service.hostTimingAvailable() && service.isMetronomePlaying()
                     && juce::approximatelyEqual(service.hostTempo(), 137.0)
                     && service.hostTimeSignature() == "7/8" && audio.getMagnitude(0, 128) > 0.0f,
                 "processBlock must pass the host playhead to the metronome engine");
    for (const auto event : midi)
        ok &= expect(!event.getMessage().isMidiClock() && !event.getMessage().isMidiStart()
                         && !event.getMessage().isMidiStop(), "host sync must not emit MIDI clock or transport");
    playhead.position.setIsPlaying(false);
    audio.clear();
    midi.clear();
    processor.processBlock(audio, midi);
    ok &= expect(!service.isMetronomePlaying() && audio.getMagnitude(0, 128) == 0.0f,
                 "host Stop must reach the engine through processBlock");
    processor.releaseResources();
    processor.setPlayHead(nullptr);

    return ok ? 0 : 1;
}
