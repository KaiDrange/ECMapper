#pragma once
#include <JuceHeader.h>
#include "Enums.h"

namespace ecm {

struct Utils {
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
