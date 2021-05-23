#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "../Models/Layout.h"
#include "EigenharpKeyComponent.h"
#include "MidiMessageSectionComponent.h"
#include "Utility.h"

class KeymapPanelComponent  : public PanelComponent,
                              public juce::MidiKeyboardStateListener,
                              public juce::KeyListener,
                              public MidiMessageSectionComponent::Listener {
public:
    KeymapPanelComponent(DeviceType model, float widthFactor, float heightFactor, juce::ValueTree parentTree);
    ~KeymapPanelComponent() override;
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    bool keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) override;

    void resized() override;
    Layout layout;

private:
    KeyConfig *selectedKey = nullptr;
    std::vector<EigenharpKeyComponent*> keys;
    juce::DrawablePath *keyImgNormal, *keyImgOver, *keyImgDown, *keyImgOn;
    juce::TextButton colourMenuButton;
    juce::TextButton zoneMenuButton;
    juce::TextButton mapTypeMenuButton;
    MidiMessageSectionComponent midiMessageSectionComponent;
    void valuesChanged(MidiMessageSectionComponent*) override;


    juce::DrawablePath* createBtnImage(juce::Colour colour);
    void enableDisableMenuButtons(bool enable);
    void showHidePanels();
    void deselectAllOtherKeys(const EigenharpKeyComponent *key);
    void createKeys();
    int getRowNumber(int keyIndex);
    int navigateNormalKeys(const juce::KeyPress &key, int selectedKeyIndex);
    int navigatePercKeys(const juce::KeyPress &key, int selectedKeyIndex);
    int navigateButtons(const juce::KeyPress &key, int selectedKeyIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeymapPanelComponent)
};
