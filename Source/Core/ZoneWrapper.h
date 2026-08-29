#pragma once
#include <JuceHeader.h>
#include "Enums.h"

namespace ecm {

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

    static MidiChannelType getMidiChannelType(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState);
    static void setMidiChannelType(InstrumentType deviceType, Zone zone, MidiChannelType midiChannelType, juce::ValueTree& rootState);
    static MidiValue getMidiValue(InstrumentType deviceType, Zone zone, juce::Identifier childId, MidiValue defaultValue, juce::ValueTree& rootState);
    static int getTranspose(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState);
    static void setTranspose(InstrumentType deviceType, Zone zone, int value, juce::ValueTree& rootState);
    static int getKeyPitchbend(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState);
    static void setKeyPitchbend(InstrumentType deviceType, Zone zone, int value, juce::ValueTree& rootState);
    static int getChannelMaxPitchbend(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState);
    static void setChannelMaxPitchbend(InstrumentType deviceType, Zone zone, int value, juce::ValueTree& rootState);
    static void setEnabled(InstrumentType deviceType, Zone zone, bool enabled, juce::ValueTree& rootState);
    static bool getEnabled(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState);
    static void setMidiValue(InstrumentType deviceType, Zone zone, juce::Identifier childId, MidiValue midiValue, juce::ValueTree& rootState);
    static InstrumentType getInstrumentTypeFromTree(juce::ValueTree tree);

    static void addListener(InstrumentType deviceType, juce::ValueTree::Listener* listener, juce::ValueTree& rootState);

    static inline const MidiValue default_pressure = { MidiValueType::ChannelAftertouch, 0 };
    static inline const MidiValue default_roll = { MidiValueType::Pitchbend, 0 };
    static inline const MidiValue default_yaw = { MidiValueType::CC, 74 };
    static inline const MidiValue default_strip1Rel = { MidiValueType::Off, 0 };
    static inline const MidiValue default_strip1Abs = { MidiValueType::Off, 0 };
    static inline const MidiValue default_strip2Rel = { MidiValueType::Off, 0 };
    static inline const MidiValue default_strip2Abs = { MidiValueType::Off, 0 };
    static inline const MidiValue default_breath = { MidiValueType::CC, 2 };

private:
    static juce::ValueTree getZoneTree(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState);
    
    static constexpr bool default_enabled = true;
    static constexpr int default_transpose = 0;
    static constexpr int default_keyPitchbend = 1;
    static constexpr int default_channelMaxPitchbend = 12;
    static constexpr MidiChannelType default_midiChannelType = MidiChannelType::MPE_Low;
};

} // namespace ecm
