#pragma once
#include <JuceHeader.h>
#include "../Models/LayoutWrapper.h"
#include "../Models/ZoneWrapper.h"
#include "../Models/SettingsWrapper.h"
#include "../Models/Enums.h"
#include "../UI/Utility.h"


class ConfigLookup {
public:
    ConfigLookup(DeviceType deviceType, juce::AudioProcessorValueTreeState &pluginState);
    void updateAll();
    void updateKey(juce::ValueTree keytree);
    void updateBreath(Zone zone);
    void updateStrips(Zone zone);

    struct Key {
        LayoutWrapper::KeyId keyId;
        EigenharpKeyType keyType = EigenharpKeyType::Normal;
        KeyMappingType mapType = KeyMappingType::None;
        int notes[4] = { -1, -1, -1, -1 };
        MidiChannelType output = MidiChannelType::Undefined;
        ZoneWrapper::MidiValue pressure;
        ZoneWrapper::MidiValue roll;
        ZoneWrapper::MidiValue yaw;
        float pbRange = 0.0f;
        int cmdCC = 0;
        int cmdOn = 0;
        int cmdOff = 0;
        int cmdType = 0; // none = 0, latch = 1, momentary = 2, trigger = 3
        int msgType = 0; // none = 0, CC = 1, PC = 2, Realtime = 3, AllNotesOff = 4
        KeyColour keyColour = KeyColour::Off;
    };
    
    struct Breath {
        ZoneWrapper::MidiValue midiValue;
        int channel = 0;
    };
    
    struct Strip {
        ZoneWrapper::MidiValue absMidiValue;
        ZoneWrapper::MidiValue relMidiValue;
        int channel = 0;
    };

    Key keys[3][120];
    Breath breath[3];
    Strip strip1[3];
    Strip strip2[3];

private:
    DeviceType deviceType;
    juce::AudioProcessorValueTreeState &pluginState;
};
