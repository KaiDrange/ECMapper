#include "MidiGenerator.h"

MidiGenerator::MidiGenerator(ConfigLookup (&configLookups)[3]) {
    this->configLookups = configLookups;
}

MidiGenerator::~MidiGenerator() {
    delete lowerChanAssigner;
    delete upperChanAssigner;
}

void MidiGenerator::start() {
    int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount();
    mpeZone.setLowerZone(lowerChannelCount, 2, SettingsWrapper::getLowerMPEPB());
    lowerChanAssigner = new juce::MPEChannelAssigner(mpeZone.getLowerZone());
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount();
        mpeZone.setUpperZone(upperChannelCount, 2, SettingsWrapper::getUpperMPEPB());
        upperChanAssigner = new juce::MPEChannelAssigner(mpeZone.getUpperZone());
    }
    else
        upperChanAssigner = nullptr;
    
    configLookups[0].updateAll();
    configLookups[1].updateAll();
    configLookups[2].updateAll();

    samplesSinceLastBreathMsg = 0;
    breathMessageCount = 0;
    stripMessageCount[0] = 0;
    stripMessageCount[1] = 0;
    for (int i = 0; i < 16; i++) {
        currentStripPBperChannel[i] = 0;
        currentKeyPBperChannel[i] = 0;
    }
    
    initialized = true;
}

void MidiGenerator::stop() {
    initialized = false;
    if (lowerChanAssigner != nullptr)
        delete lowerChanAssigner;
    if (upperChanAssigner != nullptr)
        delete upperChanAssigner;
    lowerChanAssigner = nullptr;
    upperChanAssigner = nullptr;
}

void MidiGenerator::processOSCMessage(OSC::Message &oscMsg, OSC::Message &outgoingOscMsg, juce::MidiBuffer &midiBuffer) {
    if (!initialized)
        return;
    
    switch (oscMsg.type) {
        case OSC::MessageType::Key: {
                int deviceIndex = (int)oscMsg.device -1;
                KeyState *keyState = &keyStates[deviceIndex][oscMsg.course][oscMsg.key];
                keyState->ehPressure = oscMsg.pressure;
                keyState->ehYaw = oscMsg.yaw;
                keyState->ehRoll = oscMsg.roll;

                ConfigLookup::Key keyLookup = configLookups[deviceIndex].keys[oscMsg.course][oscMsg.key];
                if (keyLookup.output == MidiChannelType::Undefined)
                    break;
                
                if (keyLookup.mapType == KeyMappingType::Note)
                    processNoteKey(oscMsg, keyLookup, keyState, midiBuffer);
                else if (keyLookup.mapType == KeyMappingType::MidiMsg)
                    processCmdKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer);
            
            }
            break;
        case OSC::MessageType::Breath: {
                breathMessageCount++;
                int deviceIndex = (int)oscMsg.device -1;
                ehBreath[deviceIndex] = std::abs((int)(oscMsg.value - 2048))*2;
                if (breathMessageCount == 16) {
                    createBreath(deviceIndex, configLookups[deviceIndex], midiBuffer);
                }
            }
            break;
        case OSC::MessageType::Strip: {
                int stripIndex = oscMsg.strip - 1;
                stripMessageCount[stripIndex]++;
                int deviceIndex = (int)oscMsg.device -1;
                bool stripOff = oscMsg.value == 2048;
                ehStrips[stripIndex][deviceIndex] = stripOff ? 0.0f : std::max((((int)oscMsg.value) - 150) * 1.5f, 0.0f);
                if (stripOff)
                    relStart_ehStrips[stripIndex][deviceIndex] = -1;
                else if (relStart_ehStrips[stripIndex][deviceIndex] < 0)
                    relStart_ehStrips[stripIndex][deviceIndex] = ehStrips[stripIndex][deviceIndex];

                for (int i = 0; i < 3; i++) {
                    if (!stripOff) {
                        createStripAbsolute(deviceIndex, stripIndex, i, configLookups[deviceIndex], midiBuffer);
                    }
                    createStripRelative(deviceIndex, stripIndex, i, configLookups[deviceIndex], midiBuffer);
                }
                stripMessageCount[stripIndex] = 0;
            }
            break;
        default:
            break;
    }
}

void MidiGenerator::processNoteKey(OSC::Message &oscMsg, ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    state->messageCount++;

    if (!oscMsg.active) {
        createNoteOff(keyLookup, state, buffer);
    }
    else if (state->status == KeyStatus::Off && oscMsg.active) {
        state->status = KeyStatus::Pending;
    }
    else if (state->messageCount == 4 && state->status == KeyStatus::Pending && oscMsg.active) {
        createNoteOn(keyLookup, state, buffer);
    }
    else if (state->messageCount == 16 && state->status != KeyStatus::Pending) {
        createNoteHold(keyLookup, state, buffer);
    }
}

void MidiGenerator::processCmdKey(OSC::Message &oscMsg, OSC::Message &outgoingOscMsg, ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    if (!oscMsg.active) {
        if (keyLookup.cmdType == 2) // only momentary creates off msg on release
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg);
    }
    else if (state->status == KeyStatus::Off && oscMsg.active) {
        if (keyLookup.cmdType == 1 && state->isLatchOn)
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg);
        else
            createMidiMsgOn(keyLookup, state, buffer, outgoingOscMsg);
    }
    state->status = oscMsg.active ? KeyStatus::Active : KeyStatus::Off;
}

void MidiGenerator::reduceBreath(juce::MidiBuffer &buffer) {
    for (int i = 0; i < 3; i++) {
        if (ehBreath[i] <= 0) {
            ehBreath[i] = 0;
            continue;
        }
        
        ehBreath[i] -= 20;
        ehBreath[i] = std::max<int>(ehBreath[i], 0);
        createBreath(i, configLookups[i], buffer);
    }
}

void MidiGenerator::createBreath(int deviceIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer) {

    addMidiValueMessage(keyLookup.breath[0].channel, ehBreath[deviceIndex], keyLookup.breath[0].midiValue, 1.0f, 0, buffer, false);
    addMidiValueMessage(keyLookup.breath[1].channel, ehBreath[deviceIndex], keyLookup.breath[1].midiValue, 1.0f, 0, buffer, false);
    addMidiValueMessage(keyLookup.breath[2].channel, ehBreath[deviceIndex], keyLookup.breath[2].midiValue, 1.0f, 0, buffer, false);

    breathMessageCount = 0;
    samplesSinceLastBreathMsg = 0;
}

void MidiGenerator::createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer) {
    
    stripIndex == 0
        ? addStripValueMessage(keyLookup.strip1[zoneIndex].channel, ehStrips[stripIndex][deviceIndex], keyLookup.strip1[zoneIndex].absMidiValue, buffer, false)
        : addStripValueMessage(keyLookup.strip2[zoneIndex].channel, ehStrips[stripIndex][deviceIndex], keyLookup.strip2[zoneIndex].absMidiValue, buffer, false);
}

void MidiGenerator::createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup &keyLookup,
    juce::MidiBuffer &buffer) {
    int relValue;

    if (relStart_ehStrips[stripIndex][deviceIndex] < 0) {
        relValue = 1;
        currentStripPBperChannel[keyLookup.strip1[zoneIndex].channel] = 0;
    }
    else
        relValue = relStart_ehStrips[stripIndex][deviceIndex] - ehStrips[stripIndex][deviceIndex];

    stripIndex == 0
        ? addStripValueMessage(keyLookup.strip1[zoneIndex].channel, relValue, keyLookup.strip1[zoneIndex].relMidiValue, buffer, true)
        : addStripValueMessage(keyLookup.strip2[zoneIndex].channel, relValue, keyLookup.strip2[zoneIndex].relMidiValue, buffer, true);
}

void MidiGenerator::createNoteOn(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    if (keyLookup.output == MidiChannelType::MPE_Low)
        state->midiChannel = lowerChanAssigner->findMidiChannelForNewNote(keyLookup.note);
    else if (keyLookup.output == MidiChannelType::MPE_High) {
        if (upperChanAssigner != nullptr)
            state->midiChannel = upperChanAssigner->findMidiChannelForNewNote(keyLookup.note);
    }
    else
        state->midiChannel = (int)keyLookup.output;
    
    createNoteHold(keyLookup, state, buffer);
    auto vel = juce::MPEValue::from7BitInt(unipolar(state->ehPressure*4)*126+1);
    auto noteOnMsg = juce::MidiMessage::noteOn(state->midiChannel, keyLookup.note, vel.asUnsignedFloat());
    buffer.addEvent(noteOnMsg, buffer.getLastEventTime()+1);
    
    state->status = KeyStatus::Active;
}

void MidiGenerator::createNoteOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    if (keyLookup.output == MidiChannelType::MPE_Low)
        lowerChanAssigner->noteOff(keyLookup.note, channel);
    else if (keyLookup.output == MidiChannelType::MPE_High) {
        if (upperChanAssigner != nullptr)
            upperChanAssigner->noteOff(keyLookup.note, channel);
    }

    
    auto noteOffMsg = juce::MidiMessage::noteOff(channel, keyLookup.note, 0.0f);
    buffer.addEvent(noteOffMsg, buffer.getLastEventTime()+1);
    addMidiValueMessage(channel, 0, keyLookup.pressure, keyLookup.pbRange, keyLookup.note, buffer, false);
    addMidiValueMessage(channel, 0, keyLookup.roll, keyLookup.pbRange, keyLookup.note, buffer, true);
    addMidiValueMessage(channel, 0, keyLookup.yaw, keyLookup.pbRange, keyLookup.note, buffer, true);
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiGenerator::createMidiMsgOn(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg) {
    state->isLatchOn = true;
    if (keyLookup.output == MidiChannelType::MPE_Low)
        state->midiChannel = 1;
    else if (keyLookup.output == MidiChannelType::MPE_High) {
        state->midiChannel = 16;
    }
    else
        state->midiChannel = (int)keyLookup.output;

    if (keyLookup.msgType == 4) {
        for (int i = 1; i < 17; i++)
            buffer.addEvent(juce::MidiMessage::allNotesOff(i), buffer.getLastEventTime()+1);
    }
    else if (keyLookup.msgType == 1)
         buffer.addEvent(juce::MidiMessage::controllerEvent(state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOn), buffer.getLastEventTime()+1);
    else if (keyLookup.msgType == 2)
        buffer.addEvent(juce::MidiMessage::programChange(state->midiChannel, keyLookup.cmdOn), buffer.getLastEventTime()+1);
    else if (keyLookup.msgType == 3) {
        switch (keyLookup.cmdOn) {
            case 1:
                buffer.addEvent(juce::MidiMessage::midiStart(), buffer.getLastEventTime()+1);
                break;
            case 2:
                buffer.addEvent(juce::MidiMessage::midiStop(), buffer.getLastEventTime()+1);
                break;
            case 3:
                buffer.addEvent(juce::MidiMessage::midiContinue(), buffer.getLastEventTime()+1);
                break;
            default:
                break;
        }
    }
    
    state->status = KeyStatus::Active;

    if (keyLookup.cmdType == 1) {
        outgoingOscMsg.type = OSC::MessageType::LED;
        outgoingOscMsg.value = 3;
    }
}

void MidiGenerator::createMidiMsgOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg) {
    if (keyLookup.cmdType != 3) { // "trigger" commands shouldn´t send anything on key off
        if (keyLookup.msgType == 4) {
            for (int i = 1; i < 17; i++)
                buffer.addEvent(juce::MidiMessage::allNotesOff(i), buffer.getLastEventTime()+1);
        }
        else if (keyLookup.msgType == 1)
             buffer.addEvent(juce::MidiMessage::controllerEvent(state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOff), buffer.getLastEventTime()+1);
        else if (keyLookup.msgType == 2)
            buffer.addEvent(juce::MidiMessage::programChange(state->midiChannel, keyLookup.cmdOff), buffer.getLastEventTime()+1);
        else if (keyLookup.msgType == 3) {
            switch (keyLookup.cmdOff) {
                case 1:
                    buffer.addEvent(juce::MidiMessage::midiStart(), buffer.getLastEventTime()+1);
                    break;
                case 2:
                    buffer.addEvent(juce::MidiMessage::midiStop(), buffer.getLastEventTime()+1);
                    break;
                case 3:
                    buffer.addEvent(juce::MidiMessage::midiContinue(), buffer.getLastEventTime()+1);
                    break;
                default:
                    break;
            }
        }
    }
    
    state->status = KeyStatus::Off;
    state->isLatchOn = false;
    if (keyLookup.cmdType == 1) {
        outgoingOscMsg.type = OSC::MessageType::LED;
        outgoingOscMsg.value = (unsigned int)keyLookup.keyColour;
    }
}

void MidiGenerator::createNoteHold(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    addMidiValueMessage(channel, state->ehRoll, keyLookup.roll, keyLookup.pbRange, keyLookup.note, buffer, true);
    addMidiValueMessage(channel, state->ehYaw, keyLookup.yaw, keyLookup.pbRange, keyLookup.note, buffer, true);
    addMidiValueMessage(channel, state->ehPressure, keyLookup.pressure, keyLookup.pbRange, keyLookup.note, buffer, false);
    state->messageCount = 0;
}

void MidiGenerator::addMidiValueMessage(int channel, int ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer &buffer, bool isBipolar) {
    if (midiValue.valueType != MidiValueType::Off) {
        juce::MidiMessage msg;
        if (midiValue.valueType == MidiValueType::Pitchbend) {
            currentKeyPBperChannel[channel-1] = (calculatePitchBendCurve(bipolar(ehValue*1.7f))*pbRange)*0x1fff;
            auto pb = juce::MPEValue::from14BitInt(std::max(std::min(currentKeyPBperChannel[channel-1] + currentStripPBperChannel[channel-1] + 0x1fff, 16383), 0));
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
            msg = juce::MidiMessage::aftertouchChange(channel, noteNo, at.as7BitInt());
        }
        else if (midiValue.valueType == MidiValueType::CC) {
            auto cc = isBipolar ? juce::MPEValue::from7BitInt(bipolar(ehValue*1.7f)*63+64)
                                : juce::MPEValue::from7BitInt(unipolar(ehValue)*127);
            msg = juce::MidiMessage::controllerEvent(channel, midiValue.ccNo, cc.as7BitInt());
        }
        buffer.addEvent(msg, buffer.getLastEventTime()+1);
    }
}

void MidiGenerator::addStripValueMessage(int channel, int ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer &buffer, bool isBipolar) {
    if (midiValue.valueType != MidiValueType::Off) {
        juce::MidiMessage msg;
        if (midiValue.valueType == MidiValueType::Pitchbend) {
            currentStripPBperChannel[channel-1] = isBipolar ? (calculatePitchBendCurve(bipolar(ehValue*1.7f)))*0x1fff : (unipolar(ehValue))*0x1fff;
            auto pb = juce::MPEValue::from14BitInt(std::max(std::min(currentKeyPBperChannel[channel-1] + currentStripPBperChannel[channel-1] + 0x1fff, 16383), 0));
            msg = juce::MidiMessage::pitchWheel(channel, pb.as14BitInt());
        }
        else if (midiValue.valueType == MidiValueType::ChannelAftertouch) {
            auto at = isBipolar ? juce::MPEValue::from7BitInt(bipolar(ehValue*1.7f)*63+64)
                                : juce::MPEValue::from7BitInt(unipolar(ehValue*1.7f)*127);
            msg = juce::MidiMessage::channelPressureChange(channel, at.as7BitInt());
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
