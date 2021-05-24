#include "MidiGenerator.h"

MidiGenerator::MidiGenerator(juce::ValueTree uiSettings) {
    for (int i = 0; i < 3; i++)
        keyConfigLookups[i] = new KeyConfigLookup(uiSettings.getChildWithName("layout" + juce::String(i+1)));
    mpeZone.setLowerZone(15, 2, 12);
    chanAssigner = new juce::MPEChannelAssigner(mpeZone.getLowerZone());
}

MidiGenerator::~MidiGenerator() {
    delete chanAssigner;
    for (int i = 0; i < 3; i++)
        delete keyConfigLookups[i];
}

void MidiGenerator::processOSCMessage(OSC::Message &oscMsg, juce::MidiBuffer &midiBuffer) {
    switch (oscMsg.type) {
            case OSC::MessageType::Key: {
                int deviceIndex = (int)oscMsg.device -1;
                KeyState *keyState = &keyStates[deviceIndex][oscMsg.course][oscMsg.key];
                keyState->messageCount++;
                keyState->ehPressure = oscMsg.pressure;
                keyState->ehYaw = oscMsg.yaw;
                keyState->ehRoll = oscMsg.roll;

                KeyConfigLookup::Key *keyLookup = &keyConfigLookups[deviceIndex]->keys[oscMsg.course][oscMsg.key];
                if (keyState->status == KeyStatus::Off && oscMsg.active) {
                    keyState->status = KeyStatus::Pending;
                }
                else if (keyState->messageCount == 4 && keyState->status == KeyStatus::Pending && oscMsg.active) {
                    createNoteOn(keyLookup->note, keyState, midiBuffer);
                }
                else if (keyState->status == KeyStatus::Active && !oscMsg.active) {
                    createNoteOff(keyLookup->note, keyState, midiBuffer);
                }
                else if (keyState->messageCount == 16 && keyState->status != KeyStatus::Pending) {
                    createNoteHold(keyLookup->note, keyState, midiBuffer);
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
    buffer.addEvent(noteOnMsg, buffer.getLastEventTime()+1);
    
    state->status = KeyStatus::Active;
}

void MidiGenerator::createNoteOff(int note, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    chanAssigner->noteOff(note, channel);
    
    auto noteOffMsg = juce::MidiMessage::noteOff(channel, note, 0.0f);
    auto pressMsg = juce::MidiMessage::aftertouchChange(channel, note, 0);

    buffer.addEvent(noteOffMsg, 0);
    buffer.addEvent(pressMsg, 1);
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiGenerator::createNoteHold(int note, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    auto pb = juce::MPEValue::from14BitInt((calculatePitchBendCurve(bipolar(state->ehRoll*1.7f))/12.0f)*0x1fff + 0x1fff);
    auto press = juce::MPEValue::from7BitInt(unipolar(state->ehPressure)*127);
    auto timbre = juce::MPEValue::from7BitInt(bipolar(state->ehYaw)*63+63);
    
    auto timbreMsg = juce::MidiMessage::controllerEvent(channel, 74, timbre.as7BitInt());
    auto pressMsg = juce::MidiMessage::aftertouchChange(channel, note, press.as7BitInt());
    auto pbMsg = juce::MidiMessage::pitchWheel(channel, pb.as14BitInt());

    buffer.addEvent(timbreMsg, 0);
    buffer.addEvent(pressMsg, 1);
    buffer.addEvent(pbMsg, 2);
    state->messageCount = 0;
}

void MidiGenerator::createLayoutRPNs(juce::MidiBuffer &buffer) {
    buffer.clear();
    auto buff = juce::MPEMessages::setZoneLayout(mpeZone);
    buffer.addEvents(buffer, 0, -1, 0);
}

float MidiGenerator::calculatePitchBendCurve(float value) {
    return clamp((tan(value)/3.14159265f) * 2.0f, -1.0, 1.0);
}
