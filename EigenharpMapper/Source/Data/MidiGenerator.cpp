#include "MidiGenerator.h"

MidiGenerator::MidiGenerator(KeyConfigLookup (&keyConfigLookups)[3]) {
    this->keyConfigLookups = keyConfigLookups;
}

MidiGenerator::~MidiGenerator() {
    delete lowChanAssigner;
    delete highChanAssigner;
}

void MidiGenerator::start() {
    int lowChannelCount = SettingsWrapper::getLowMPEToChannel() -1;
    mpeZone.setLowerZone(lowChannelCount, 2, SettingsWrapper::getLowMPEPB());
    lowChanAssigner = new juce::MPEChannelAssigner(mpeZone.getLowerZone());
    if (lowChannelCount < 14) {
        mpeZone.setUpperZone(14-lowChannelCount, 2, SettingsWrapper::getHighMPEPB());
        highChanAssigner = new juce::MPEChannelAssigner(mpeZone.getUpperZone());
    }
    else
        highChanAssigner = nullptr;
    initialized = true;
}

void MidiGenerator::stop() {
    initialized = false;
    delete lowChanAssigner;
    delete highChanAssigner;
    lowChanAssigner = nullptr;
    highChanAssigner = nullptr;
}

void MidiGenerator::processOSCMessage(OSC::Message &oscMsg, juce::MidiBuffer &midiBuffer) {
    if (!initialized)
        return;
    
    switch (oscMsg.type) {
            case OSC::MessageType::Key: {
                int deviceIndex = (int)oscMsg.device -1;
                KeyState *keyState = &keyStates[deviceIndex][oscMsg.course][oscMsg.key];
                keyState->messageCount++;
                keyState->ehPressure = oscMsg.pressure;
                keyState->ehYaw = oscMsg.yaw;
                keyState->ehRoll = oscMsg.roll;

                KeyConfigLookup::Key keyLookup = keyConfigLookups[deviceIndex].keys[oscMsg.course][oscMsg.key];
                if (keyLookup.output == MidiChannelType::Undefined)
                    break;
                
                if (keyState->status == KeyStatus::Off && oscMsg.active) {
                    keyState->status = KeyStatus::Pending;
                }
                else if (keyState->messageCount == 4 && keyState->status == KeyStatus::Pending && oscMsg.active) {
                    createNoteOn(keyLookup, keyState, midiBuffer);
                }
                else if (keyState->status == KeyStatus::Active && !oscMsg.active) {
                    createNoteOff(keyLookup, keyState, midiBuffer);
                }
                else if (keyState->messageCount == 16 && keyState->status != KeyStatus::Pending) {
                    createNoteHold(keyLookup, keyState, midiBuffer);
                }
            }
            break;
        default:
            break;
    }
}

void MidiGenerator::createNoteOn(KeyConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    if (keyLookup.output == MidiChannelType::MPE_Low)
        state->midiChannel = lowChanAssigner->findMidiChannelForNewNote(keyLookup.note);
    else if (keyLookup.output == MidiChannelType::MPE_High) {
        if (highChanAssigner != nullptr)
            state->midiChannel = highChanAssigner->findMidiChannelForNewNote(keyLookup.note);
    }
    else
        state->midiChannel = (int)keyLookup.output;
    
    createNoteHold(keyLookup, state, buffer);
    auto vel = juce::MPEValue::from7BitInt(unipolar(state->ehPressure*4)*126+1);
    auto noteOnMsg = juce::MidiMessage::noteOn(state->midiChannel, keyLookup.note, vel.asUnsignedFloat());
    buffer.addEvent(noteOnMsg, buffer.getLastEventTime()+1);
    
    state->status = KeyStatus::Active;
}

void MidiGenerator::createNoteOff(KeyConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    if (keyLookup.output == MidiChannelType::MPE_Low)
        lowChanAssigner->noteOff(keyLookup.note, channel);
    else if (keyLookup.output == MidiChannelType::MPE_High) {
        if (highChanAssigner != nullptr)
            highChanAssigner->noteOff(keyLookup.note, channel);
    }

    auto noteOffMsg = juce::MidiMessage::noteOff(channel, keyLookup.note, 0.0f);
    auto pressMsg = juce::MidiMessage::aftertouchChange(channel, keyLookup.note, 0);

    buffer.addEvent(noteOffMsg, 0);
    buffer.addEvent(pressMsg, 1);
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiGenerator::createNoteHold(KeyConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    auto pb = juce::MPEValue::from14BitInt((calculatePitchBendCurve(bipolar(state->ehRoll*1.7f))*keyLookup.pbRange)*0x1fff + 0x1fff);
    auto press = juce::MPEValue::from7BitInt(unipolar(state->ehPressure)*127);
    auto timbre = juce::MPEValue::from7BitInt(bipolar(state->ehYaw)*63+63);
    
    auto timbreMsg = juce::MidiMessage::controllerEvent(channel, 74, timbre.as7BitInt());
    auto pressMsg = juce::MidiMessage::aftertouchChange(channel, keyLookup.note, press.as7BitInt());
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
