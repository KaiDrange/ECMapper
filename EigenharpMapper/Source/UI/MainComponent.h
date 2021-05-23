#pragma once
#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "KeymapPanelComponent.h"
#include "TabPage.h"

class MainComponent : public juce::Component {
public:
    MainComponent(juce::ValueTree state);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    Layout* getLayout(DeviceType deviceType);

    void addListener(juce::ValueTree::Listener *listener);
    juce::ValueTree uiSettings;

private:
    juce::Identifier id_uiSettings = "uiSettings";
    juce::TabbedComponent tabs;
    TabPage *tabPages[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
