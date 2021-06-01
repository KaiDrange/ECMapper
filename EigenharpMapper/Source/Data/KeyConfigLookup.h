#pragma once
#include <JuceHeader.h>
#include "../Models/LayoutWrapper.h"
#include "../Models/ZoneWrapper.h"
#include "../Models/SettingsWrapper.h"
#include "../Models/Enums.h"


class KeyConfigLookup {
public:
    KeyConfigLookup(DeviceType deviceType);
    void updateAll();
    void updateKey(juce::ValueTree keytree);
    void updateBreath(Zone zone);
    
    struct Key {
        KeyMappingType mapType = KeyMappingType::None;
        int note = 0;
        MidiChannelType output = MidiChannelType::Undefined;
        ZoneWrapper::MidiValue pressure;
        ZoneWrapper::MidiValue roll;
        ZoneWrapper::MidiValue yaw;
        float pbRange = 0.0f;
    };
    
    struct Breath {
        ZoneWrapper::MidiValue midiValue;
        int channel = 0;
    };

    Key keys[3][120];
    Breath breath[3];

private:
    DeviceType deviceType;
};
