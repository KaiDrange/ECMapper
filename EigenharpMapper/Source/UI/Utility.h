#pragma once
#include <JuceHeader.h>
#include "../Models/Enums.h"

class Utility {
public:
    static juce::Colour zoneEnumToColour(Zone zone);
    static juce::Colour keyColourEnumToColour(KeyColour colour);
private:
};

const static juce::Colour zoneColours[5] = {
    juce::Colours::black,
    juce::Colours::darkblue,
    juce::Colours::maroon,
    juce::Colours::darkorange,
    juce::Colours::white
};

const static juce::Colour keyColours[4] {
    juce::Colour(0x00000000),
    juce::Colour(0xFF00FF00),
    juce::Colour(0xFFFFFF00),
    juce::Colour(0xFFFF0000)
};




