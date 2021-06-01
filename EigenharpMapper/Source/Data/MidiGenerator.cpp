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
                else if (keyState->status != KeyStatus::Off && !oscMsg.active) {
                    createNoteOff(keyLookup, keyState, midiBuffer);
                }
                else if (keyState->messageCount == 16 && keyState->status != KeyStatus::Pending) {
                    createNoteHold(keyLookup, keyState, midiBuffer);
                }
            }
            break;
        case OSC::MessageType::Breath: {
                breathMessageCount++;
                int deviceIndex = (int)oscMsg.device -1;
                ehBreath[deviceIndex] = std::abs((int)(oscMsg.value - 2048))*2;
                if (breathMessageCount == 16) {
                    createBreath(deviceIndex, keyConfigLookups[deviceIndex], midiBuffer);
                }
            }
            break;
        default:
            break;
    }
}

void MidiGenerator::reduceBreath(juce::MidiBuffer &buffer) {
    samplesSinceLastBreathMsg = 0;
    for (int i = 0; i < 3; i++) {
        if (ehBreath[i] <= 0) {
            ehBreath[i] = 0;
            continue;
        }
        
        ehBreath[i] -= 20;
        ehBreath[i] = std::max<int>(ehBreath[i], 0);
        createBreath(i, keyConfigLookups[i], buffer);
    }
}

void MidiGenerator::createBreath(int deviceIndex, KeyConfigLookup &keyLookup, juce::MidiBuffer &buffer) {

    addMidiValueMessage(keyLookup.breath[0].channel, ehBreath[deviceIndex], keyLookup.breath[0].midiValue, keyLookup.keys[0][0], buffer, false);
    addMidiValueMessage(keyLookup.breath[1].channel, ehBreath[deviceIndex], keyLookup.breath[1].midiValue, keyLookup.keys[0][0], buffer, false);
    addMidiValueMessage(keyLookup.breath[2].channel, ehBreath[deviceIndex], keyLookup.breath[2].midiValue, keyLookup.keys[0][0], buffer, false);

    breathMessageCount = 0;
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
    buffer.addEvent(noteOffMsg, 0);
    addMidiValueMessage(channel, 0, keyLookup.pressure, keyLookup, buffer, false);
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiGenerator::createNoteHold(KeyConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    addMidiValueMessage(channel, state->ehRoll, keyLookup.roll, keyLookup, buffer, true);
    addMidiValueMessage(channel, state->ehYaw, keyLookup.yaw, keyLookup, buffer, true);
    addMidiValueMessage(channel, state->ehPressure, keyLookup.pressure, keyLookup, buffer, false);
    state->messageCount = 0;
}

void MidiGenerator::addMidiValueMessage(int channel, int ehValue, ZoneWrapper::MidiValue midiValue, KeyConfigLookup::Key &keyLookup, juce::MidiBuffer &buffer, bool isBipolar) {
    if (midiValue.valueType != MidiValueType::Off) {
        juce::MidiMessage msg;
        if (midiValue.valueType == MidiValueType::Pitchbend) {
            auto pb = isBipolar ? juce::MPEValue::from14BitInt((calculatePitchBendCurve(bipolar(ehValue*1.7f))*keyLookup.pbRange)*0x1fff + 0x1fff) : juce::MPEValue::from14BitInt((unipolar(ehValue)*keyLookup.pbRange)*0x1fff + 0x1fff);
            msg = juce::MidiMessage::pitchWheel(channel, pb.as14BitInt());
        }
        else if (midiValue.valueType == MidiValueType::ChannelAftertouch) {
            auto at = isBipolar ? juce::MPEValue::from7BitInt(bipolar(ehValue*1.7f)*63+64)
                                : juce::MPEValue::from7BitInt(unipolar(ehValue*1.7f)*127);
            msg = juce::MidiMessage::channelPressureChange(channel, at.as7BitInt());
        }
        else if (midiValue.valueType == MidiValueType::PolyAftertouch) {
            auto at = isBipolar ? juce::MPEValue::from7BitInt(bipolar(ehValue*1.7f)*63+64)
                                : juce::MPEValue::from7BitInt(unipolar(ehValue*1.7f)*127);
            msg = juce::MidiMessage::aftertouchChange(channel, keyLookup.note, at.as7BitInt());
        }
        else if (midiValue.valueType == MidiValueType::CC) {
            auto cc = isBipolar ? juce::MPEValue::from7BitInt(bipolar(ehValue*1.7f)*63+64)
                                : juce::MPEValue::from7BitInt(unipolar(ehValue)*127);
            msg = juce::MidiMessage::controllerEvent(channel, midiValue.ccNo, cc.as7BitInt());
        }
        buffer.addEvent(msg, buffer.getLastEventTime()+1);
    }
}

void MidiGenerator::createLayoutRPNs(juce::MidiBuffer &buffer) {
    buffer.clear();
    auto buff = juce::MPEMessages::setZoneLayout(mpeZone);
    buffer.addEvents(buffer, 0, -1, 0);
}

float MidiGenerator::calculatePitchBendCurve(float value) {
    return clamp((tan(value)/3.14159265f) * 2.0f, -1.0, 1.0);
}
