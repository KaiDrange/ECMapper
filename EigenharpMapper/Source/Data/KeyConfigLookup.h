#pragma once
#include <JuceHeader.h>
#include "../Models/LayoutWrapper.h"
#include "../Models/Enums.h"


class KeyConfigLookup {
public:
    KeyConfigLookup(DeviceType deviceType);
    void updateAll();
    
    struct Key {
        KeyMappingType mapType = KeyMappingType::None;
        int note = 0;
        int output = -1;
        int midiChannel = 1;
        int transpose = 0;
    };

    Key keys[3][120];

private:
    DeviceType deviceType;
//    juce::ValueTree layoutTree;
//    juce::ValueTree zoneTrees[3];
};
