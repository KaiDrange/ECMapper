#pragma once

#include <JuceHeader.h>
#include "LayoutComponent.h"
#include "ZonePanelComponent.h"
#include "../Models/Enums.h"
#include "../Data/FileUtil.h"
#include "../Models/SettingsWrapper.h"

class TabPage : public juce::Component {
public:
    TabPage(int tabIndex, DeviceType deviceType, juce::AudioProcessorValueTreeState &pluginState);
    ~TabPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DeviceType deviceType;
    LayoutComponent *layoutPanel;
    ZonePanelComponent *zonePanels[3];

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton saveMappingButton;
    juce::TextButton loadMappingButton;
    juce::ToggleButton controlLightsButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};
