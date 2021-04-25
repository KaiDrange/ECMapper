#pragma once

#include <JuceHeader.h>
#include "Enums.h"
#include "EigenharpMapping.h"

const static juce::Colour zoneColours[4] = {
    juce::Colours::black,
    juce::Colours::maroon,
    juce::Colours::darkcyan,
    juce::Colours::darkorange
};

class EigenharpKeyComponent  : public juce::DrawableButton
{
public:
    EigenharpKeyComponent(const EigenharpKeyType keyType, const MappedKey *mappedKey);
    ~EigenharpKeyComponent() override;

    void paint (juce::Graphics&) override;
    int id;

private:
    EigenharpKeyType keyType;
    const MappedKey *mappedKey;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpKeyComponent)
};
