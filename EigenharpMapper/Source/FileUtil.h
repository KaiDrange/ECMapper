#pragma once
#include <JuceHeader.h>
#include "EigenharpMapping.h"
#include "Enums.h"


class FileUtil {
public:
    static juce::String openMapping(InstrumentType instrumentType);
    static bool saveMapping(juce::ValueTree valueTree, InstrumentType instrumentType);

private:
    static juce::String getFileExtension(InstrumentType instrumentType);
};
