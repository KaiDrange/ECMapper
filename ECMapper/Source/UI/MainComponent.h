#pragma once
#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "LayoutComponent.h"
#include "TabPage.h"
#include "NumberInputComponent.h"
#include "DropdownComponent.h"
#include "../Models/SettingsWrapper.h"

class MainComponent : public juce::Component, public juce::ValueTree::Listener {
public:
    MainComponent(juce::AudioProcessorValueTreeState &pluginState);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
//    void addListener(juce::ValueTree::Listener *listener);

private:
    NumberInputComponent lowerMPEVoiceCount;
    NumberInputComponent upperMPEVoiceCount;
    NumberInputComponent lowerMPEPitchbendRange;
    NumberInputComponent upperMPEPitchbendRange;
    juce::Label oscIPLabel;
    juce::TextEditor oscIPInput;

    juce::TabbedComponent tabs;
    TabPage *tabPages[3];

    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
