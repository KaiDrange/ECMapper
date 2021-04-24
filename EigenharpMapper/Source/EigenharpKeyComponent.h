#pragma once

#include <JuceHeader.h>
#include "Enums.h"

class EigenharpKeyComponent  : public juce::DrawableButton
{
public:
    EigenharpKeyComponent(EigenharpKeyType keyType);
    ~EigenharpKeyComponent() override;

    void paint (juce::Graphics&) override;

private:
    EigenharpKeyType keyType;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpKeyComponent)
};
