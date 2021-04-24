#pragma once
#include <JuceHeader.h>
#include "Enums.h"

struct MappedKey {
    KeyColour colour = KeyColour::Off;
    KeyMappingType mappingType;
    juce::String mapping;
};

class EigenharpMapping {
public:
    EigenharpMapping(InstrumentType instrumentType);
    ~EigenharpMapping();

    int keyCount;
    int percKeyCount;
    int buttonCount;
    int stripCount;
    int keyRowCount;
    int keyRowLengths[5];
    InstrumentType instrumentType;
    MappedKey *mappedKeys;
    MappedKey *mappedPercKeys;
    MappedKey *mappedButtons;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpMapping)
};
