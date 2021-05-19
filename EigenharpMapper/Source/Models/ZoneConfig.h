#pragma once
#include <JuceHeader.h>
#include "Enums.h"

class ZoneConfig {
public:
    ZoneConfig(Zone zone);
    Zone zone;
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
    
private:
    juce::ValueTree valueTree;
    juce::Identifier id_enabled = "enabled";
    juce::Identifier id_transpose = "transpose";
    juce::Identifier id_globalPitchbend = "globalPitchbend";
    juce::Identifier id_keyPitchbend = "keyPitchbend";
    juce::Identifier id_midiChannelType = "midiChannelType";
};
