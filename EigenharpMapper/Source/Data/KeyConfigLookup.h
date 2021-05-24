#pragma once
#include <JuceHeader.h>
#include "../Models/KeyConfig.h"
#include "../Models/Enums.h"


class KeyConfigLookup {
public:
    KeyConfigLookup(juce::ValueTree layoutTree, juce::ValueTree zoneTree1, juce::ValueTree zoneTree2, juce::ValueTree zoneTree3);
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
    juce::ValueTree layoutTree;
    juce::ValueTree zoneTrees[3];
};
