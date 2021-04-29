#pragma once

#include <JuceHeader.h>
#include "KeymapPanelComponent.h"
#include "EigenharpMapping.h"
#include "Enums.h"
#include "FileUtil.h"

class TabPage : public juce::Component
{
public:
    TabPage(InstrumentType model);
    ~TabPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String text;

private:
    InstrumentType model;
    KeymapPanelComponent *keymapPanel;
    EigenharpMapping *keymap;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton saveMappingButton;
    juce::TextButton loadMappingButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};
