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
    EigenharpKeyComponent *keys[120];
    EigenharpKeyComponent *percKeys[12];
    EigenharpKeyComponent *buttons[8];

    EigenharpMapping *eigenharpMapping;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeymapPanelComponent)
};
