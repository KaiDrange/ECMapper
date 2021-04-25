#pragma once
#include <JuceHeader.h>
#include "Enums.h"

struct MappedKey {
    KeyColour colour = KeyColour::Off;
    KeyMappingType mappingType;
    juce::String mapping;
    Zone zone = Zone::NoZone;
    EigenharpKeyType keyType;
};

class EigenharpMapping {
public:
    EigenharpMapping(InstrumentType instrumentType);
    ~EigenharpMapping();

    int normalKeyCount;
    int percKeyCount;
    int buttonCount;
    int stripCount;
    int keyRowCount;
    int keyRowLengths[5];
    InstrumentType instrumentType;
    MappedKey *mappedKeys;
    int getTotalKeyCount();
    int getPercKeyStartIndex();
    int getButtonStartIndex();
//    MappedKey *mappedPercKeys;
//    MappedKey *mappedButtons;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpMapping)
};
