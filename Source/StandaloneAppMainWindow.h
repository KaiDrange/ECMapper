#pragma once

#include <juceheader.h>
#include "PluginProcessor.h"
#include "UI/AppStyle.h"
#include "UI/PresetBrowserComponent.h"
#include "UI/MainMenuBarModel.h"

class StandaloneAppMainWindow : public juce::DocumentWindow,
                                private juce::ChangeListener
{
public:
    explicit StandaloneAppMainWindow (const juce::String& name);
    ~StandaloneAppMainWindow() override;

    void closeButtonPressed() override;

    void showAudioSettings();
    void showSavePresetDialog();
    void showPresetBrowser();
    void showAboutDialog();
    static void showOnlineManual();
    static void showOurMusic();
    void requestQuit();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    void updateMidiOutput();
    
    void saveAudioSettings();
    void loadAudioSettings();
    juce::File getAudioSettingsFile();

    std::unique_ptr<ECMapperAudioProcessor> processor;
    
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    ecm::AppLookAndFeel lookAndFeel;
    MainMenuBarModel menuBarModel;

    bool isUpdatingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneAppMainWindow)
};
