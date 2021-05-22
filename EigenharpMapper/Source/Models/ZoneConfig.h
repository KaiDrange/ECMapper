#pragma once
#include <JuceHeader.h>
#include "Enums.h"

class ZoneConfig {
public:
    ZoneConfig(Zone zone);
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
    juce::String getPressure() const;
    void setPressure(juce::String pressure);
    juce::String getRoll() const;
    void setRoll(juce::String roll);
    juce::String getYaw() const;
    void setYaw(juce::String yaw);
    juce::String getStrip1Rel() const;
    void setStrip1Rel(juce::String strip1rel);
    juce::String getStrip1Abs() const;
    void setStrip1Abs(juce::String strip1abs);
    juce::String getStrip2Rel() const;
    void setStrip2Rel(juce::String strip2rel);
    juce::String getStrip2Abs() const;
    void setStrip2Abs(juce::String strip2abs);
    juce::String getBreath() const;
    void setBreath(juce::String breath);
    
    void addListener(juce::ValueTree::Listener *listener);

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
    juce::ValueTree valueTree;
private:
};
