#pragma once
#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "KeymapPanelComponent.h"
#include "TabPage.h"

class MainComponent : public juce::Component {
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    juce::ValueTree* getValueTree();

private:
    juce::TabbedComponent tabs;
    TabPage *tabPages[3];
    juce::ValueTree uiSettings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
