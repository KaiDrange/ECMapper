#pragma once
#include <JuceHeader.h>
#include <math.h>
#include "ConfigLookup.h"
#include "OSCMessageQueue.h"

class MidiGenerator {
public:
    MidiGenerator(ConfigLookup (&configLookups) [3]);
    ~MidiGenerator();
    
    void processOSCMessage(OSC::Message &oscMsg, OSC::Message &outgoingOscMsg, juce::MidiBuffer &midiBuffer);
    void reduceBreath(juce::MidiBuffer &buffer);
    juce::MPEZoneLayout mpeZone;
    
    void createLayoutRPNs(juce::MidiBuffer &buffer);
    void start(juce::AudioProcessorValueTreeState &pluginState);
    void stop();
    int samplesSinceLastBreathMsg = 0;
    bool initialized = false;

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
        
//        unsigned int midiVelocity = 0;
//        unsigned int midiPressure = 0;
//        unsigned int midiRoll = 0;
//        unsigned int midiYaw = 0;
        int midiChannel = 1;
        int messageCount = 0;
        bool isLatchOn = false;
    };
    
    KeyState keyStates[3][3][120];
    
    unsigned int ehBreath[3] = { 0,0,0 };
    unsigned int ehPedal1 = 0;
    unsigned int ehPedal2 = 0;
    int ehStrips[2][3] = {{ 0, 0, 0 }, { 0, 0, 0 }};
    int relStart_ehStrips[2][3] = {{ -1, -1, -1 }, { -1, -1, -1 }};
    int currentKeyPBperChannel[16];
    int currentStripPBperChannel[16];

    juce::MPEChannelAssigner *lowerChanAssigner = nullptr;
    juce::MPEChannelAssigner *upperChanAssigner = nullptr;

    void processNoteKey(OSC::Message &oscMsg, ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void processCmdKey(OSC::Message &oscMsg, OSC::Message &outgoingOscMsg, ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void createNoteOn(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void createNoteOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void createMidiMsgOn(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg);
    void createMidiMsgOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg);
    void createNoteHold(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer);
    void addMidiValueMessage(int channel, int ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer &buffer, bool isBipolar);
    void addStripValueMessage(int channel, int ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer &buffer, bool isBipolar);
    void createBreath(int deviceIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer);
    void createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer);
    void createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer);
    void createAllNotesOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg);

    inline float clamp(float v, float mn, float mx) { return (std::max(std::min(v, mx), mn)); }
    float unipolar(int val) { return std::min(float(val) / 4096.0f, 1.0f); }
    float bipolar(int val) { return clamp(float(val) / 4096.0f, -1.0f, 1.0f); }
    float calculatePitchBendCurve(float value);

    ConfigLookup *configLookups;
    int breathMessageCount = 0;
    int stripMessageCount[2] = { 0, 0 };
    
    std::list<LayoutWrapper::KeyId> chanNotePri[16];
};
