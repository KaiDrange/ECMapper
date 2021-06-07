#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "../Models/LayoutWrapper.h"
#include "KeyConfigComponent.h"
#include "MidiMessageSectionComponent.h"
#include "Utility.h"
#include "LayoutComponent.h"

class LayoutComponent : public PanelComponent, public juce::MidiKeyboardStateListener, public juce::KeyListener, public MidiMessageSectionComponent::Listener {
public:
    LayoutComponent(DeviceType model, float widthFactor, float heightFactor);
    ~LayoutComponent() override;
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    bool keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) override;

    void resized() override;
//    Layout layout;
    
    int getNormalkeyCount() const;
    int getPercKeyCount() const;
    int getButtonCount() const;
    int getStripCount() const;
    int getKeyRowCount() const;
    const int* getKeyRowLengths() const;
    int getTotalKeyCount() const;
    int getPercKeyStartIndex() const;
    int getButtonStartIndex() const;
    
private:
    LayoutWrapper::KeyId activeKeyId;
    std::vector<KeyConfigComponent*> keys;
    juce::DrawablePath *keyImgNormal, *keyImgOver, *keyImgDown, *keyImgOn;
    juce::TextButton colourMenuButton;
    juce::TextButton zoneMenuButton;
    juce::TextButton mapTypeMenuButton;
    MidiMessageSectionComponent midiMessageSectionComponent;
    void valuesChanged(MidiMessageSectionComponent*) override;


    juce::DrawablePath* createBtnImage(juce::Colour colour);
    void enableDisableMenuButtons(bool enable);
    void showHidePanels();
    void deselectAllOtherKeys(const KeyConfigComponent *key);
    void createKeys();
    int getRowNumber(int keyIndex);
    int navigateNormalKeys(const juce::KeyPress &key, int selectedKeyIndex);
    int navigatePercKeys(const juce::KeyPress &key, int selectedKeyIndex);
    int navigateButtons(const juce::KeyPress &key, int selectedKeyIndex);
    void setKeyCounts(DeviceType deviceType);

    
    int normalKeyCount;
    int percKeyCount;
    int buttonCount;
    int stripCount;
    int keyRowCount;
    int keyRowLengths[5];
    DeviceType deviceType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayoutComponent)
};
