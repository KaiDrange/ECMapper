#pragma once
#include <array>
#include <JuceHeader.h>
#include "LayoutWrapper.h"
#include "ZoneWrapper.h"
#include "ExpressionCurve.h"
#include "ExpressionCurveWrapper.h"
#include "SettingsWrapper.h"
#include "Enums.h"
#include "Utils.h"

namespace ecm {

class ConfigLookup {
public:
    ConfigLookup(InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState, juce::CriticalSection& stateLock);
    ConfigLookup(const ConfigLookup& other);
    ConfigLookup& operator=(const ConfigLookup&) = delete;
    ConfigLookup(ConfigLookup&&) = delete;
    ConfigLookup& operator=(ConfigLookup&&) = delete;
    
    void updateAll();
    void updateKey(juce::ValueTree keytree);
    void updateKey(LayoutWrapper::KeyId keyId);
    void updateBreath(Zone zone);
    void updateStrips(Zone zone);
    void updateExpressionCurves();

    struct Key {
        LayoutWrapper::KeyId keyId;
        EigenharpKeyType keyType = EigenharpKeyType::Normal;
        KeyMappingType mapType = KeyMappingType::None;
        std::array<int, 4> notes = { -1, -1, -1, -1 };
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
        int appCtrlType = 0; // none = 0, preset = 1, transpose = 2
        int appCtrlValue = 0;
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
    ExpressionCurve expressionCurves[6];
    
    bool controlLights = true;

    juce::CriticalSection& getLock() { return lock_; }

private:
    juce::CriticalSection lock_;
    juce::CriticalSection& stateLock_;
    InstrumentType deviceType;
    juce::AudioProcessorValueTreeState& pluginState;

    void updateKeyUnlocked(LayoutWrapper::KeyId keyId);
    void updateBreathUnlocked(Zone zone);
    void updateStripsUnlocked(Zone zone);
    void updateExpressionCurvesUnlocked();
};

} // namespace ecm
