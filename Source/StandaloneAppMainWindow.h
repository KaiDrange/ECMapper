#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/AppStyle.h"
#include "UI/MainMenuBarModel.h"

class StandaloneAppMainWindow : public juce::DocumentWindow,
                                private juce::ChangeListener,
                                private juce::ValueTree::Listener,
                                private juce::universal_midi_packets::EndpointsListener
{
public:
    explicit StandaloneAppMainWindow (const juce::String& name);
    ~StandaloneAppMainWindow() override;

    void closeButtonPressed() override;

    void showAudioSettings();
    void showPresetBrowser();
    void showMidiMonitor();
    void showAboutDialog();
    static void showOnlineManual();
    static void showOurMusic();
    static void requestQuit();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property) override;
    void endpointsChanged() override;

private:
    void updateMidiOutput();
    
    void saveAudioSettings();
    void loadAudioSettings();
    static juce::File getAudioSettingsFile();
    
    void saveAppState();
    void loadAppState();
    static juce::File getAppStateFile();

    std::unique_ptr<ECMapperAudioProcessor> processor;
    
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    
    ecm::AppLookAndFeel lookAndFeel;
    MainMenuBarModel menuBarModel;

    bool isUpdatingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneAppMainWindow)
};
