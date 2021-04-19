#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "EigenharpMapping.h"
#include "KeymapPanelComponent.h"
#include "TabPage.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TabbedComponent tabs;
    TabPage *tabPages[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
