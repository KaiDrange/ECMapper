#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class StandaloneAppMainWindow : public juce::DocumentWindow
{
public:
    explicit StandaloneAppMainWindow (const juce::String& name);
    ~StandaloneAppMainWindow() override;

    void closeButtonPressed() override;

private:
    std::unique_ptr<ECMapperAudioProcessor> processor;
    
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneAppMainWindow)
};
