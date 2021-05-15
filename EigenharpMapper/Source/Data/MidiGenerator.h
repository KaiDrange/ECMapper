#pragma once
#include <JuceHeader.h>
#include "KeyConfigLookup.h"
#include "OSCMessageQueue.h"

class MidiGenerator {
public:
    const int decimation = 4;
    KeyConfigLookup keyConfigLookup;
    void processOSCMessage(OSC::Message &oscMsg, juce::MidiBuffer &midiBuffer);

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
    };
    
    KeyState keyStates[3][120];
    
    unsigned int ehBreath = 0;
    unsigned int ehPedal1 = 0;
    unsigned int ehPedal2 = 0;
    unsigned int ehStrip1 = 0;
    unsigned int ehStrip2 = 0;

    unsigned int midiBreath = 0;
    unsigned int midiStrip1 = 0;
    unsigned int midiStrip2 = 0;
    unsigned int midiPedal1 = 0;
    unsigned int midiPedal2 = 0;
    
};
