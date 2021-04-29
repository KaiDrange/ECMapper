#pragma once
#include <JuceHeader.h>
#include "EigenharpMapping.h"
#include "Enums.h"


class FileUtil {
public:
    static juce::String openMapping();
    static bool saveMapping(juce::ValueTree valueTree);

private:
};
