#include "Utility.h"

juce::Colour Utility::zoneEnumToColour(Zone zone) {
    return zoneColours[(int)zone];
}

juce::Colour Utility::keyColourEnumToColour(KeyColour colour) {
    return keyColours[(int)colour];
}
