#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "EigenharpMapping.h"
#include "EigenharpKeyComponent.h"

class KeymapPanelComponent  : public PanelComponent, public juce::MidiKeyboardStateListener
{
public:
    KeymapPanelComponent(EigenharpMapping *eigenharpMapping, float widthFactor, float heightFactor);
    ~KeymapPanelComponent() override;
    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    void resized() override;

private:
    MappedKey *selectedKey = NULL;
    EigenharpKeyComponent *keys[120+12+8];
    juce::DrawablePath *keyImgNormal, *keyImgOver, *keyImgDown, *keyImgOn;
    juce::TextButton colourMenuButton;
    juce::TextButton zoneMenuButton;
    juce::TextButton mapTypeMenuButton;
    EigenharpMapping *eigenharpMapping;

    juce::DrawablePath* createBtnImage(juce::Colour colour);
    void enableDisableMenuButtons(bool enable);
    void deselectAllOtherKeys(const EigenharpKeyComponent *key);
    void createKeys();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeymapPanelComponent)
};
