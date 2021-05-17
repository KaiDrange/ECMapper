#pragma once
#include <JuceHeader.h>
#include "../Models/KeyConfig.h"
#include "../Models/Enums.h"
#include "../Models/Layout.h"


class KeyConfigLookup {
public:
    void updateAll();
    void setLayout(Layout *layout);
    
    struct Key {
        KeyMappingType mapType = KeyMappingType::None;
        int note = 0;
        int output = -1;
        int midiChannel = 1;
        int transpose = 0;
    };

    Key keys[3][120];

private:
    Layout *layout;
};
