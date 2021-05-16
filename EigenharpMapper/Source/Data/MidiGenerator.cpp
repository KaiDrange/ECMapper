#include "MidiGenerator.h"

MidiGenerator::MidiGenerator() {
    mpeZone.setLowerZone(15, 12, 2);
    chanAssigner = new juce::MPEChannelAssigner(mpeZone.getLowerZone());
}

MidiGenerator::~MidiGenerator() {
    delete chanAssigner;
}

void MidiGenerator::processOSCMessage(OSC::Message &oscMsg, juce::MidiBuffer &midiBuffer) {
    switch (oscMsg.type) {
            case OSC::MessageType::Key: {
                KeyState *keyState = &keyStates[oscMsg.course][oscMsg.key];
                keyState->messageCount++;
                keyState->ehPressure = oscMsg.pressure;
                keyState->ehYaw = oscMsg.yaw;
                keyState->ehRoll = oscMsg.roll;

                KeyConfigLookup::Key *keyLookup = &keyConfigLookup.keys[oscMsg.course][oscMsg.key];
                if (keyState->status == KeyStatus::Off && oscMsg.active) {
                    keyState->status = KeyStatus::Pending;
                }
                else if (keyState->messageCount == 4 && keyState->status == KeyStatus::Pending && oscMsg.active) {
                    createNoteOn(keyLookup->note, keyState, midiBuffer);
                }
                else if (keyState->status == KeyStatus::Active && !oscMsg.active) {
                    createNoteOff(keyLookup->note, keyState, midiBuffer);
                }
                else if (keyState->messageCount == 16) {
                    createNoteHold(Note, keyState, midiBuffer);
                }
            }
            break;
        default:
            break;
    }
}

void MidiGenerator::createNoteOn(int note, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = chanAssigner->findMidiChannelForNewNote(note);
    state->midiChannel = channel;
    createNoteHold(note, state, buffer);
    auto vel = juce::MPEValue::from7BitInt(unipolar(state->ehPressure*4)*126+1);
    auto noteOnMsg = juce::MidiMessage::noteOn(channel, note, vel.asUnsignedFloat());
    buffer.addEvent(noteOnMsg, 0);
    
    state->status = KeyStatus::Active;
}

void MidiGenerator::createNoteOff(int note, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    chanAssigner->noteOff(note, channel);
    
    auto noteOffMsg = juce::MidiMessage::noteOff(channel, note, 0.0f);
    auto pressMsg = juce::MidiMessage::aftertouchChange(channel, note, 0);

    buffer.addEvent(noteOffMsg, 0);
    buffer.addEvent(pressMsg, 0);
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiGenerator::createNoteHold(int note, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    auto pb = juce::MPEValue::from14BitInt(bipolar(state->ehRoll)*0x1fff + 0x1fff);
    auto press = juce::MPEValue::from7BitInt(unipolar(state->ehPressure)*127);
    auto timbre = juce::MPEValue::from7BitInt(bipolar(state->ehYaw)*63+63);
    
    auto timbreMsg = juce::MidiMessage::controllerEvent(channel, 74, timbre.as7BitInt());
    auto pressMsg = juce::MidiMessage::aftertouchChange(channel, note, press.as7BitInt());
    auto pbMsg = juce::MidiMessage::pitchWheel(channel, pb.as14BitInt());

    buffer.addEvent(timbreMsg, 0);
    buffer.addEvent(pressMsg, 0);
    buffer.addEvent(pbMsg, 0);
    state->messageCount = 0;
}
