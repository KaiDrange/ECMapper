#pragma once
#include <JuceHeader.h>
#include "Enums.h"

class ZoneConfig {
public:
    ZoneConfig(int tabNo, Zone zone, juce::ValueTree parentTree);

    struct MidiValue {
        MidiValueType valueType = MidiValueType::CC;
        int ccNo = 0;
    };

    Zone zone;
    juce::ValueTree getValueTree() const;
    
    bool isEnabled() const;
    void setEnabled(bool enabled);
    int getTranspose() const;
    void setTranspose(int transpose);
    int getGlobalPitchbend() const;
    void setGlobalPitchbend(int pitchbend);
    int getKeyPitchbend() const;
    void setKeyPitchbend(int pitchbend);
    MidiChannelType getMidiChannelType() const;
    void setMidiChannelType(MidiChannelType type);

    void addListener(juce::ValueTree::Listener *listener);
    
    MidiValue getMidiValue(juce::Identifier childId) const;
    void setMidiValue(juce::Identifier childId, MidiValue midiValue);

    juce::Identifier id_enabled = "enabled";
    juce::Identifier id_transpose = "transpose";
    juce::Identifier id_globalPitchbend = "globalPitchbend";
    juce::Identifier id_keyPitchbend = "keyPitchbend";
    juce::Identifier id_midiChannelType = "midiChannelType";
    juce::Identifier id_pressure = "pressure";
    juce::Identifier id_roll = "roll";
    juce::Identifier id_yaw = "yaw";
    juce::Identifier id_strip1Rel = "strip1Rel";
    juce::Identifier id_strip1Abs = "strip1Abs";
    juce::Identifier id_strip2Rel = "strip2Rel";
    juce::Identifier id_strip2Abs = "strip2Abs";
    juce::Identifier id_breath = "breath";
    juce::Identifier id_midiValType = "midiValType";
    juce::Identifier id_midiCCNo = "midiCCNo";
    juce::Identifier id_midiVal = "midiVal";
    juce::ValueTree valueTree;
private:
};
