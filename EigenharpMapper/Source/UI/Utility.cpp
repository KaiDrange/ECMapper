#include "Utility.h"

juce::Colour Utility::zoneEnumToColour(Zone zone) {
    return zoneColours[zone];
}

juce::Colour Utility::keyColourEnumToColour(KeyColour colour) {
    return keyColours[colour];
}
