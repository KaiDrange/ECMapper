#include "MidiGenerator.h"

void MidiGenerator::processOSCMessage(OSC::Message &oscMsg, juce::MidiBuffer &midiBuffer) {
    switch (oscMsg.type) {
            case OSC::MessageType::Key: {
                KeyState *keyState = &keyStates[oscMsg.course][oscMsg.key];
                KeyConfigLookup::Key *keyLookup = &keyConfigLookup.keys[oscMsg.course][oscMsg.key];
                if (keyState->status != KeyStatus::Active && oscMsg.active) {
                    midiBuffer.addEvent(juce::MidiMessage::noteOn(keyLookup->midiChannel, keyLookup->note, 0.8f), 0);
                    keyState->status = KeyStatus::Active;
                }
                else if (keyState->status == KeyStatus::Active && !oscMsg.active) {
                    midiBuffer.addEvent(juce::MidiMessage::noteOff(keyLookup->midiChannel, keyLookup->note, 0.8f), 0);
                    keyState->status = Off;
                }
            }
            break;
        default:
            break;
    }
}

