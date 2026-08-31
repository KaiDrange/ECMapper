#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/AppStyle.h"

class StandaloneAppMainWindow : public juce::DocumentWindow,
                                 private juce::ChangeListener
{
public:
    explicit StandaloneAppMainWindow (const juce::String& name);
    ~StandaloneAppMainWindow() override;

    void closeButtonPressed() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void updateMidiOutput();
    
    void saveAudioSettings();
    void loadAudioSettings();
    void savePluginState();
    void loadPluginState();
    juce::File getAudioSettingsFile();
    juce::File getPluginStateFile();

    std::unique_ptr<ECMapperAudioProcessor> processor;
    
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    ecm::AppLookAndFeel lookAndFeel;

    bool isUpdatingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneAppMainWindow)
};
