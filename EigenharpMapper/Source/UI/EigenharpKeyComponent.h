#pragma once

#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/EigenharpMapping.h"

const static juce::Colour zoneColours[5] = {
    juce::Colours::black,
    juce::Colours::darkblue,
    juce::Colours::maroon,
    juce::Colours::darkorange,
    juce::Colours::white
};

class EigenharpKeyComponent  : public juce::DrawableButton {
public:
    EigenharpKeyComponent(const EigenharpKeyType keyType, const MappedKey *mappedKey);
    ~EigenharpKeyComponent() override;

    void paint (juce::Graphics&) override;
    juce::String getKeyId() const;

private:
    EigenharpKeyType keyType;
    const MappedKey *mappedKey;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpKeyComponent)
};
