#include "MidiGenerator.h"

MidiGenerator::MidiGenerator(ConfigLookup (&configLookups)[3]) : velocityCurve(0.0, 0.0, 0.0, 1.0, 0.5, 0.6, 1.0, 1.0) {
    this->configLookups = configLookups;
}

MidiGenerator::~MidiGenerator() {
    delete lowerChanAssigner;
    delete upperChanAssigner;
}

void MidiGenerator::start(juce::AudioProcessorValueTreeState &pluginState) {
    int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount(pluginState.state);
    mpeZone.setLowerZone(lowerChannelCount, 2, SettingsWrapper::getLowerMPEPB(pluginState.state));
    if (lowerChanAssigner != nullptr) {
        delete lowerChanAssigner;
        lowerChanAssigner = nullptr;
    }

    if (upperChanAssigner != nullptr) {
        delete upperChanAssigner;
        upperChanAssigner = nullptr;
    }

    lowerChanAssigner = new juce::MPEChannelAssigner(mpeZone.getLowerZone());
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(pluginState.state);
        mpeZone.setUpperZone(upperChannelCount, 2, SettingsWrapper::getUpperMPEPB(pluginState.state));
        upperChanAssigner = new juce::MPEChannelAssigner(mpeZone.getUpperZone());
    }
    else
        upperChanAssigner = nullptr;
    
    configLookups[0].updateAll();
    configLookups[1].updateAll();
    configLookups[2].updateAll();

    stripMessageCount[0] = 0;
    stripMessageCount[1] = 0;
    for (int i = 0; i < 16; i++) {
        currentStripPBperChannel[i] = 0;
        currentKeyPBperChannel[i] = 0;
        chanNotePri[i].clear();
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
                keyState->ehYaw = oscMsg.yaw;
                keyState->ehRoll = oscMsg.roll;
                keyState->ehPressureHistory.push_back(oscMsg.pressure);
                while (keyState->ehPressureHistory.size() > PRESSURE_HISTORY_LENGTH)
                    keyState->ehPressureHistory.pop_front();

                ConfigLookup::Key keyLookup = configLookups[deviceIndex].keys[oscMsg.course][oscMsg.key];
                if (keyLookup.output == MidiChannelType::Undefined)
                    break;
                
                if (keyLookup.mapType == KeyMappingType::Note || keyLookup.mapType == KeyMappingType::Chord)
                    processNoteKey(oscMsg, keyLookup, keyState, midiBuffer);
                else if (keyLookup.mapType == KeyMappingType::MidiMsg)
                    processCmdKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer);
            }
            break;
        case OSC::MessageType::Breath: {
                int deviceIndex = (int)oscMsg.device -1;
                unsigned int prevBreathValue = ehBreath[deviceIndex];
                ehBreath[deviceIndex] = std::abs((int)(oscMsg.value - 2048))*2;
                if ((ehBreath[deviceIndex] > BREATH_ZERO_THRESHOLD) || (ehBreath[deviceIndex] < BREATH_ZERO_THRESHOLD && prevBreathValue > 0)) {
                    createBreath(deviceIndex, configLookups[deviceIndex], midiBuffer);
                }
            }
            break;
        case OSC::MessageType::Strip: {
                int stripIndex = oscMsg.strip - 1;
                stripMessageCount[stripIndex]++;
                int deviceIndex = (int)oscMsg.device -1;
                bool stripOff = !oscMsg.active;
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
    else if (state->messageCount == PRESSURE_HISTORY_LENGTH && state->status == KeyStatus::Pending && oscMsg.active) {
        createNoteOn(keyLookup, state, buffer);
    }
    else if (state->messageCount == 64 && state->status != KeyStatus::Pending) {
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
        if (ehBreath[i] == 0)
            continue;
        
        ehBreath[i] = ehBreath[i] > BREATH_ZERO_THRESHOLD ? ehBreath[i] - 20 : 0;
        createBreath(i, configLookups[i], buffer);
    }
}

void MidiGenerator::createBreath(int deviceIndex, ConfigLookup &keyLookup, juce::MidiBuffer &buffer) {
    ehBreath[deviceIndex] = ehBreath[deviceIndex] < BREATH_ZERO_THRESHOLD ? 0 : ehBreath[deviceIndex] - BREATH_ZERO_THRESHOLD;
    
    addMidiValueMessage(keyLookup.breath[0].channel, ehBreath[deviceIndex]*3, keyLookup.breath[0].midiValue, 1.0f, 0, buffer, false);
    addMidiValueMessage(keyLookup.breath[1].channel, ehBreath[deviceIndex]*3, keyLookup.breath[1].midiValue, 1.0f, 0, buffer, false);
    addMidiValueMessage(keyLookup.breath[2].channel, ehBreath[deviceIndex]*3, keyLookup.breath[2].midiValue, 1.0f, 0, buffer, false);
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
    if (keyLookup.output == MidiChannelType::MPE_Low && lowerChanAssigner != nullptr)
        state->midiChannel = lowerChanAssigner->findMidiChannelForNewNote(keyLookup.notes[0]);
    else if (keyLookup.output == MidiChannelType::MPE_High && upperChanAssigner != nullptr)
        state->midiChannel = upperChanAssigner->findMidiChannelForNewNote(keyLookup.notes[0]);
    else
        state->midiChannel = (int)keyLookup.output;
    if (state->midiChannel > 0)
        chanNotePri[state->midiChannel-1].push_front(keyLookup.keyId);

    createNoteHold(keyLookup, state, buffer);
    auto vel = calculateNoteOnVelocity(state);
    int eventTime = buffer.getLastEventTime()+8;
    for (int i = 0; i < 4; i++) {
        if (keyLookup.notes[i] > -1) {
            int existingSameNoteCount = countPlayingNoteMatches(state->midiChannel, keyLookup.notes[i]);
            if (existingSameNoteCount == 0) {
                auto noteOnMsg = juce::MidiMessage::noteOn(state->midiChannel, keyLookup.notes[i], vel.asUnsignedFloat());
                buffer.addEvent(noteOnMsg, eventTime);
            }
            MidiNote midiNote;
            midiNote.noteNumber = keyLookup.notes[i];
            midiNote.channnel = state->midiChannel;
            playingNotes.push_back(midiNote);
        }
    }
    
    state->status = KeyStatus::Active;
}

void MidiGenerator::createNoteOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer) {
    int channel = state->midiChannel;
    if (keyLookup.output == MidiChannelType::MPE_Low && lowerChanAssigner != nullptr)
        lowerChanAssigner->noteOff(keyLookup.notes[0], channel);
    else if (keyLookup.output == MidiChannelType::MPE_High && upperChanAssigner != nullptr)
        upperChanAssigner->noteOff(keyLookup.notes[0], channel);

    if (state->midiChannel > 0) {
        for (auto it = chanNotePri[state->midiChannel-1].begin(); it != chanNotePri[state->midiChannel-1].end(); it++) {
            if (it->equals(keyLookup.keyId)) {
                chanNotePri[state->midiChannel-1].erase(it);
                break;
            }
        }
    }

    int eventTime = buffer.getLastEventTime()+8;
    for (int i = 0; i < 4; i++) {
        if (keyLookup.notes[i] > -1) {
            int matchingNotes = countPlayingNoteMatches(channel, keyLookup.notes[i]);

            if (matchingNotes < 2) {
                auto noteOffMsg = juce::MidiMessage::noteOff(channel, keyLookup.notes[i], calculateNoteOffVelocity(state).asUnsignedFloat());
                buffer.addEvent(noteOffMsg, eventTime);
            }
            removeOneNoteMatch(channel, keyLookup.notes[i]);
        }
    }
    
    if (chanNotePri[state->midiChannel-1].empty()) {
        addMidiValueMessage(channel, 0, keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], buffer, false);
        addMidiValueMessage(channel, 0, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
        addMidiValueMessage(channel, 0, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
    }
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

int MidiGenerator::countPlayingNoteMatches(int channel, int noteNumber) {
    int matchingNotes = 0;
    std::list<MidiNote>::iterator it = playingNotes.begin();
    
    while (it != playingNotes.end()) {
        if (it->channnel == channel && it->noteNumber == noteNumber) {
            matchingNotes++;
        }
        it++;
    }
    return matchingNotes;
}

void MidiGenerator::removeOneNoteMatch(int channel, int noteNumber) {
    std::list<MidiNote>::iterator it = playingNotes.begin();
    
    while (it != playingNotes.end()) {
        if (it->channnel == channel && it->noteNumber == noteNumber) {
            playingNotes.erase(it);
            break;
        }
        it++;
    }
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

    if (keyLookup.msgType == 4)
        createAllNotesOff(keyLookup, state, buffer, outgoingOscMsg);
    else if (keyLookup.msgType == 1)
         buffer.addEvent(juce::MidiMessage::controllerEvent(state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOn), buffer.getLastEventTime()+8);
    else if (keyLookup.msgType == 2)
        buffer.addEvent(juce::MidiMessage::programChange(state->midiChannel, keyLookup.cmdOn), buffer.getLastEventTime()+8);
    else if (keyLookup.msgType == 3) {
        switch (keyLookup.cmdOn) {
            case 1:
                buffer.addEvent(juce::MidiMessage::midiStart(), buffer.getLastEventTime()+8);
                break;
            case 2:
                buffer.addEvent(juce::MidiMessage::midiStop(), buffer.getLastEventTime()+8);
                break;
            case 3:
                buffer.addEvent(juce::MidiMessage::midiContinue(), buffer.getLastEventTime()+8);
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

void MidiGenerator::createAllNotesOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg) {
    for (int i = 1; i < 17; i++) {
        buffer.addEvent(juce::MidiMessage::allNotesOff(i), buffer.getLastEventTime()+8);
        chanNotePri[i-1].clear();
    }
    if (lowerChanAssigner != nullptr)
        lowerChanAssigner->allNotesOff();
    if (upperChanAssigner != nullptr)
        upperChanAssigner->allNotesOff();
    playingNotes.clear();
}

void MidiGenerator::createMidiMsgOff(ConfigLookup::Key &keyLookup, KeyState *state, juce::MidiBuffer &buffer, OSC::Message &outgoingOscMsg) {
    if (keyLookup.cmdType != 3) { // "trigger" commands shouldn´t send anything on key off
        if (keyLookup.msgType == 4) {
            for (int i = 1; i < 17; i++)
                buffer.addEvent(juce::MidiMessage::allNotesOff(i), buffer.getLastEventTime()+8);
        }
        else if (keyLookup.msgType == 1)
             buffer.addEvent(juce::MidiMessage::controllerEvent(state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOff), buffer.getLastEventTime()+8);
        else if (keyLookup.msgType == 2)
            buffer.addEvent(juce::MidiMessage::programChange(state->midiChannel, keyLookup.cmdOff), buffer.getLastEventTime()+8);
        else if (keyLookup.msgType == 3) {
            switch (keyLookup.cmdOff) {
                case 1:
                    buffer.addEvent(juce::MidiMessage::midiStart(), buffer.getLastEventTime()+8);
                    break;
                case 2:
                    buffer.addEvent(juce::MidiMessage::midiStop(), buffer.getLastEventTime()+8);
                    break;
                case 3:
                    buffer.addEvent(juce::MidiMessage::midiContinue(), buffer.getLastEventTime()+8);
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
    if (state->midiChannel > 0 && (chanNotePri[state->midiChannel-1].empty() || chanNotePri[state->midiChannel-1].front().equals(keyLookup.keyId))) {
        addMidiValueMessage(channel, state->ehRoll, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
        addMidiValueMessage(channel, state->ehYaw, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
        addMidiValueMessage(channel, state->ehPressureHistory.back(), keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], buffer, false);
    }
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
        buffer.addEvent(msg, buffer.getLastEventTime()+8);
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
        buffer.addEvent(msg, buffer.getLastEventTime()+8);
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

juce::MPEValue MidiGenerator::calculateNoteOnVelocity(KeyState *state) {
    if (state->ehPressureHistory.size() < PRESSURE_HISTORY_LENGTH)
        return juce::MPEValue::from7BitInt(1);
    
    auto listPos = state->ehPressureHistory.begin();
    std::advance(listPos, 1);
    unsigned int val1 = (*listPos)*0.5;
    std::advance(listPos, 1);
    val1 += (*listPos)*0.5;
    std::advance(listPos, 2);
    unsigned int val2 = (*listPos)*0.5;
    std::advance(listPos, 1);
    val2 += (*listPos)*0.5;
    int tableIndex = (val2 - val1);
    tableIndex = std::max(0, std::min(velocityCurve.TABLE_LENGTH-1, tableIndex));
    return juce::MPEValue::from7BitInt(velocityCurve.getTableValue(tableIndex)*126+1);
}

juce::MPEValue MidiGenerator::calculateNoteOffVelocity(KeyState *state) {
    if (state->ehPressureHistory.size() < PRESSURE_HISTORY_LENGTH)
        return juce::MPEValue::from7BitInt(0);
    
    unsigned int pressure = state->ehPressureHistory.front();
    return juce::MPEValue::from7BitInt(unipolar(pressure*10.0f)*127);
}
