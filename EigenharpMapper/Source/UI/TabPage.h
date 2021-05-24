#pragma once

#include <JuceHeader.h>
#include "LayoutComponent.h"
#include "ZonePanelComponent.h"
#include "../Models/Layout.h"
#include "../Models/Enums.h"
#include "../Data/FileUtil.h"

class TabPage : public juce::Component {
public:
    TabPage(int tabIndex, DeviceType model, juce::ValueTree parentTree);
    ~TabPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    Layout* getLayout();
    ZoneConfig* getZoneConfig(Zone zone);
    juce::String text;

private:
    DeviceType model;
    LayoutComponent *layoutPanel;
    ZonePanelComponent *zonePanels[3];
    juce::TextEditor oscIPInput;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton saveMappingButton;
    juce::TextButton loadMappingButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};
