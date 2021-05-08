#pragma once

#include <JuceHeader.h>
#include "KeymapPanelComponent.h"
#include "ZonePanelComponent.h"
#include "../Models/EigenharpMapping.h"
#include "../Models/Enums.h"
#include "../Data/FileUtil.h"

class TabPage : public juce::Component {
public:
    TabPage(InstrumentType model);
    ~TabPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String text;

private:
    InstrumentType model;
    KeymapPanelComponent *keymapPanel;
    ZonePanelComponent *zonePanels[3];
    juce::TextEditor oscIPInput;

    EigenharpMapping *keymap;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton saveMappingButton;
    juce::TextButton loadMappingButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};
