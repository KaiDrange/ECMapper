#pragma once
#include <JuceHeader.h>
#include <functional>
#include "TabPage.h"
#include "CorePage.h"
#include "NumberInputComponent.h"
#include "PresetBrowserComponent.h"
#include "../Core/HardwareService.h"

class ECMapperAudioProcessor;

namespace ecm {

class PresetComboBox : public juce::ComboBox
{
public:
    std::function<void()> onBrowseRequested;

    void mouseDown(const juce::MouseEvent& event) override
    {
        juce::ignoreUnused(event);
        if (onBrowseRequested)
            onBrowseRequested();
    }
};

class MainComponent : public juce::Component, public juce::ValueTree::Listener, private juce::Timer {
public:
    MainComponent(juce::AudioProcessorValueTreeState& pluginStateToUse, HardwareService& hardwareService, ECMapperAudioProcessor& processorToUse, juce::AudioDeviceManager* deviceManagerToUse = nullptr);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) override;

private:
    void timerCallback() override;
    void selectTab(int index);
    void refreshPresetComboBox();
    void refreshFromState();
    void handlePresetSelectionChanged();
    void showPresetBrowser();

    PresetComboBox presetComboBox;
    NumberInputComponent lowerMPEVoiceCount;
    NumberInputComponent upperMPEVoiceCount;
    NumberInputComponent lowerMPEPitchbendRange;
    NumberInputComponent upperMPEPitchbendRange;
    juce::TextButton mpeModeButton;
    juce::TextButton midi20ModeButton;

    juce::TextButton communicationTabButton;
    juce::TextButton alphaTabButton;
    juce::TextButton tauTabButton;
    juce::TextButton picoTabButton;
    std::unique_ptr<CorePage> corePage;
    std::unique_ptr<TabPage> alphaPage;
    std::unique_ptr<TabPage> tauPage;
    std::unique_ptr<TabPage> picoPage;
    int currentTabIndex = 0;
    bool midi2ModeEnabled = false;
    bool ignorePresetComboChange_ = false;
    ECMapperAudioProcessor& processor;

    juce::AudioProcessorValueTreeState& pluginState;
    juce::AudioDeviceManager* deviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace ecm
