#pragma once
#include <JuceHeader.h>
#include <math.h>
#include "ConfigLookup.h"
#include "OSCMessageQueue.h"

class MidiGenerator {
public:
    MidiGenerator(ConfigLookup (&configLookups) [3]);
    ~MidiGenerator();
    
    void processOSCMessage(OSC::Message &oscMsg, juce::MidiBuffer &midiBuffer);
    void reduceBreath(juce::MidiBuffer &buffer);
    juce::MPEZoneLayout mpeZone;
    
    void createLayoutRPNs(juce::MidiBuffer &buffer);
    void start();
    void stop();
    int samplesSinceLastBreathMsg = 0;

private:
    enum KeyStatus {
        Off = 0,
        Pending = 1,
        Active = 2
    };

    struct KeyState {
        KeyStatus status = KeyStatus::Off;
        unsigned int ehPressure = 0;
        int ehRoll = 0;
        int ehYaw = 0;
        
        unsigned int midiVelocity = 0;
        unsigned int midiPressure = 0;
        unsigned int midiRoll = 0;
        unsigned int midiYaw = 0;
        int midiChannel = 1;
        int messageCount = 0;
    };
    
    KeyState keyStates[3][3][120];
    
    unsigned int ehBreath[3] = { 0,0,0 };
    unsigned int ehPedal1 = 0;
    unsigned int ehPedal2 = 0;
    unsigned int ehStrip1 = 0;
    unsigned int ehStrip2 = 0;

    unsigned int midiBreath = 0;
    unsigned int midiStrip1 = 0;
    unsigned int midiStrip2 = 0;
    unsigned int midiPedal1 = 0;
    unsigned int midiPedal2 = 0;
    
    juce::MPEChannelAssigner *lowChanAssigner;
    juce::MPEChannelAssigner *highChanAssigner;

    void createNoteOn(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void createNoteOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void createNoteHold(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void addMidiValueMessage(int channel, int ehValue, ZoneWrapper::MidiValue midiValue, ConfigLookup::Key &keyLookup, juce::MidiBuffer &buffer, bool isBipolar);
    void createBreath(int deviceIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer);
    
    inline float clamp(float v, float mn, float mx) { return (std::max(std::min(v, mx), mn)); }
    float unipolar(int val) { return std::min(float(val) / 4096.0f, 1.0f); }
    float bipolar(int val) { return clamp(float(val) / 4096.0f, -1.0f, 1.0f); }
    float calculatePitchBendCurve(float value);

    ConfigLookup *configLookups;
    bool initialized = false;
    int breathMessageCount = 0;
};
