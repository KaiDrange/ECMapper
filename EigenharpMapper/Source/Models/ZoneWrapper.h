#pragma once
#include <JuceHeader.h>
#include "Enums.h"

extern juce::ValueTree *rootState;

class ZoneWrapper {
public:
    struct MidiValue {
        MidiValueType valueType = MidiValueType::CC;
        int ccNo = 0;
    };

    static inline const juce::Identifier id_zone { "zone" };
    static inline const juce::Identifier id_device { "device" };
    static inline const juce::Identifier id_enabled { "enabled" };
    static inline const juce::Identifier id_transpose { "transpose" };
    static inline const juce::Identifier id_keyPitchbend { "keyPitchbend" };
    static inline const juce::Identifier id_channelMaxPitchbend { "channelMaxPitchbend" };
    static inline const juce::Identifier id_midiChannelType { "midiChannelType" };
    static inline const juce::Identifier id_pressure { "pressure" };
    static inline const juce::Identifier id_roll { "roll" };
    static inline const juce::Identifier id_yaw { "yaw" };
    static inline const juce::Identifier id_strip1Rel { "strip1Rel" };
    static inline const juce::Identifier id_strip1Abs { "strip1Abs" };
    static inline const juce::Identifier id_strip2Rel { "strip2Rel" };
    static inline const juce::Identifier id_strip2Abs { "strip2Abs" };
    static inline const juce::Identifier id_breath { "breath" };
    static inline const juce::Identifier id_midiValType { "midiValType" };
    static inline const juce::Identifier id_midiCCNo { "midiCCNo" };
    static inline const juce::Identifier id_midiVal { "midiVal" };

    static MidiChannelType getMidiChannelType(DeviceType deviceType, Zone zone);
    static void setMidiChannelType(DeviceType deviceType, Zone zone, MidiChannelType midiChannelType);
    static ZoneWrapper::MidiValue getMidiValue(DeviceType deviceType, Zone zone, juce::Identifier childId, ZoneWrapper::MidiValue defaultValue);
    static int getTranspose(DeviceType deviceType, Zone zone);
    static void setTranspose(DeviceType deviceType, Zone zone, int value);
    static int getKeyPitchbend(DeviceType deviceType, Zone zone);
    static void setKeyPitchbend(DeviceType deviceType, Zone zone, int value);
    static int getChannelMaxPitchbend(DeviceType deviceType, Zone zone);
    static void setChannelMaxPitchbend(DeviceType deviceType, Zone zone, int value);
    static void setEnabled(DeviceType deviceType, Zone zone, bool enabled);
    static bool getEnabled(DeviceType deviceType, Zone zone);
    static void setMidiValue(DeviceType deviceType, Zone zone, juce::Identifier childId, MidiValue midiValue);
    static DeviceType getDeviceTypeFromTree(juce::ValueTree tree);

    static void addListener(DeviceType deviceType, juce::ValueTree::Listener *listener);

    static inline const MidiValue default_pressure = { MidiValue { .valueType = MidiValueType::ChannelAftertouch, .ccNo = 0}};
    static inline const MidiValue default_roll = { MidiValue { .valueType = MidiValueType::Pitchbend, .ccNo = 0}};
    static inline const MidiValue default_yaw = { MidiValue { .valueType = MidiValueType::CC, .ccNo = 74}};
    static inline const MidiValue default_strip1Rel = { MidiValue { .valueType = MidiValueType::Off, .ccNo = 0}};
    static inline const MidiValue default_strip1Abs = { MidiValue { .valueType = MidiValueType::Off, .ccNo = 0}};
    static inline const MidiValue default_strip2Rel = { MidiValue { .valueType = MidiValueType::Off, .ccNo = 0}};
    static inline const MidiValue default_strip2Abs = { MidiValue { .valueType = MidiValueType::Off, .ccNo = 0}};
    static inline const MidiValue default_breath = { MidiValue { .valueType = MidiValueType::CC, .ccNo = 2}};

private:
    static juce::ValueTree getZoneTree(DeviceType deviceType, Zone zone);
    static inline const bool default_enabled = true;
    static inline const int default_transpose = 0;
    static inline const int default_keyPitchbend = 1;
    static inline const int default_channelMaxPitchbend = 12;
    static inline const MidiChannelType default_midiChannelType = MidiChannelType::MPE_Low;
};
