#pragma once
#include <JuceHeader.h>
#include "Enums.h"

namespace ecm {

struct Utils {
    static juce::Colour zoneEnumToColour(Zone zone) {
        switch (zone) {
            case Zone::Zone1: return juce::Colour(0xff1010bb);
            case Zone::Zone2: return juce::Colours::maroon;
            case Zone::Zone3: return juce::Colours::darkorange;
            case Zone::AllZones: return juce::Colours::white;
            default: return juce::Colours::black;
        }
    }

    static juce::Colour keyColourEnumToColour(KeyColour colour) {
        switch (colour) {
            case KeyColour::Green: return juce::Colour(0xFF00FF00);
            case KeyColour::Red: return juce::Colour(0xFFFF0000);
            case KeyColour::Yellow: return juce::Colour(0xFFFFFF00);
            default: return juce::Colour(0x00000000);
        }
    }

    static void splitString(const juce::String& text, const juce::String& separator, juce::StringArray& tokens) {
        tokens.addTokens(text, separator, "\"");
    }
};

} // namespace ecm
