#pragma once
#include <JuceHeader.h>
#include "../Models/KeyConfig.h"
#include "../Models/Enums.h"
#include "../Models/Layout.h"


class KeyConfigLookup {
public:
    void updateAll();
    void setActiveLayout(Layout *layout);
    
    enum KeyStatus {
        Off = 0,
        Pending = 1,
        Active = 2
    };

    struct Key {
        KeyMappingType mapType = KeyMappingType::None;
        int note = 0;
        KeyStatus status = KeyStatus::Off;
        int output = -1;
        int midiChannel = -1;
        int transpose = 0;
    };

    Key lookup[3][120];

private:
    Layout *layout;
};
