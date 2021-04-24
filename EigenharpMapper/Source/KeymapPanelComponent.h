#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "EigenharpMapping.h"
#include "EigenharpKeyComponent.h"

class KeymapPanelComponent  : public PanelComponent
{
public:
    KeymapPanelComponent(EigenharpMapping *eigenharpMapping, float widthFactor, float heightFactor);
    ~KeymapPanelComponent() override;

    void resized() override;

private:
    MappedKey *selectedKey = NULL;
    EigenharpKeyComponent *keys[120];
    EigenharpKeyComponent *percKeys[12];
    EigenharpKeyComponent *buttons[8];
    juce::DrawablePath *keyImgNormal, *keyImgOver, *keyImgDown, *keyImgOn;
    juce::TextButton colourMenuButton;
    EigenharpMapping *eigenharpMapping;

    juce::DrawablePath* createBtnImage(juce::Colour colour);
    void enableDisableMenuButtons(bool enable);
    void deselectAllOtherKeys(const EigenharpKeyComponent *key);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeymapPanelComponent)
};
