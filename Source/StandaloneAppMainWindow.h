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
    void showCalibrationDialog();
    void showAboutDialog();
    static void showOnlineManual();
    static void showOurMusic();
    static void requestQuit();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property) override;
    void endpointsChanged() override;

private:
    void updateMidiOutput();
    void updateMidiInputs();
    bool isMidi2ModeEnabled() const;
    void applyBufferSize(int bufferSize);
    int getRequestedBufferSize() const;
    void openStandaloneZoneMidiOutputs();
    void restoreMidiInputSelection();
    juce::String getConfiguredZoneOutputId(int zoneIndex) const;
    juce::String getConfiguredMidiInputId() const;
    
    void saveAudioSettings();
    void loadAudioSettings();
    static juce::File getAudioSettingsFile();
    
    void saveAppState();
    void loadAppState();
    static juce::File getAppStateFile();

    std::unique_ptr<ECMapperAudioProcessor> processor;
    
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    std::array<std::unique_ptr<juce::MidiOutput>, 3> standaloneZoneOutputs_;
    std::array<juce::String, 3> standaloneZoneOutputIds_;
    juce::String standaloneMidiInputId_;
    int requestedBufferSize_ = 256;
    
    ecm::AppLookAndFeel lookAndFeel;
    MainMenuBarModel menuBarModel;

    bool isUpdatingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneAppMainWindow)
};
