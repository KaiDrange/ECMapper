#pragma once
#include <JuceHeader.h>
#include "../Core/Enums.h"
#include "../Core/SettingsWrapper.h"
#include "TabButtonBarComponent.h"
#include "TabPage.h"
#include "CorePage.h"
#include "NumberInputComponent.h"
#include "../Core/HardwareService.h"

namespace ecm {

class MainComponent : public juce::Component, public juce::ValueTree::Listener {
public:
    MainComponent(juce::AudioProcessorValueTreeState& pluginState, HardwareService& hardwareService);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) override;
    
    NumberInputComponent lowerMPEVoiceCount;
    NumberInputComponent upperMPEVoiceCount;
    NumberInputComponent lowerMPEPitchbendRange;
    NumberInputComponent upperMPEPitchbendRange;
    juce::Label oscIPLabel;
    juce::TextEditor oscIPInput;

    TabButtonBarComponent tabs;
    std::unique_ptr<CorePage> corePage;
    std::unique_ptr<TabPage> alphaPage;
    std::unique_ptr<TabPage> tauPage;
    std::unique_ptr<TabPage> picoPage;

    juce::AudioProcessorValueTreeState& pluginState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace ecm
