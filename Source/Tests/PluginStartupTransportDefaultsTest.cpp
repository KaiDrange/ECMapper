#include <iostream>

#include "PluginProcessor.h"
#include "Core/SettingsWrapper.h"

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
    ECMapperAudioProcessor processor;

    bool ok = true;
    ok &= expect(!ecm::SettingsWrapper::getMidi2Mode(processor.state.state),
                 "fresh plugin state should keep MIDI 2.0 mode disabled by default");
    ok &= expect(ecm::SettingsWrapper::getPluginOutputMode(processor.state.state) == ecm::OutputTransportMode::Vst3Direct,
                 "fresh plugin state should default to VST3 Direct mode");

    return ok ? 0 : 1;
}