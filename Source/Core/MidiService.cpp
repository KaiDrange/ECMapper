#include "MidiService.h"
#include <cmath>

namespace ecm {

MidiService::MidiService(ConfigLookup (&configLookups)[3])
    : configLookups_(configLookups) {
}

MidiService::~MidiService() {
    stop();
}

void MidiService::start(juce::AudioProcessorValueTreeState& pluginState) {
    int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount(pluginState.state);
    mpeZone_.setLowerZone(lowerChannelCount, 2, SettingsWrapper::getLowerMPEPB(pluginState.state));
    
    lowerChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(mpeZone_.getLowerZone());
    
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(pluginState.state);
        mpeZone_.setUpperZone(upperChannelCount, 2, SettingsWrapper::getUpperMPEPB(pluginState.state));
        upperChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(mpeZone_.getUpperZone());
    } else {
        upperChanAssigner_.reset();
    }
    
    for (int i = 0; i < 3; ++i) {
        configLookups_[i].updateAll();
    }

    ehBreath_[0] = ehBreath_[1] = ehBreath_[2] = 0.0f;
    for (int i = 0; i < 16; ++i) {
        currentStripPBperChannel_[i] = 0;
        currentKeyPBperChannel_[i] = 0;
        chanNotePri_[i].clear();
    }
    playingNotes_.clear();
    
    initialized_ = true;
}

void MidiService::stop() {
    initialized_ = false;
    lowerChanAssigner_.reset();
    upperChanAssigner_.reset();
}

void MidiService::processMessage(osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer) {
    if (!initialized_) return;
    
    int deviceIndex = static_cast<int>(oscMsg.device) - 1;
    if (deviceIndex < 0 || deviceIndex > 2) {
        if (oscMsg.type == osc::MessageType::Key && oscMsg.active)
            juce::Logger::writeToLog("MidiService: Invalid device index: " + juce::String(deviceIndex + 1));
        return;
    }

    const juce::ScopedLock sl(configLookups_[deviceIndex].getLock());
    
    switch (oscMsg.type) {
        case osc::MessageType::Key: {
            if (oscMsg.course >= 3 || oscMsg.key >= 120) break;
            KeyState* keyState = &keyStates_[deviceIndex][oscMsg.course][oscMsg.key];
            keyState->ehYaw = oscMsg.yaw;
            keyState->ehRoll = oscMsg.roll;
            keyState->ehPressureHistory.push_back(oscMsg.pressure);
            while (keyState->ehPressureHistory.size() > PRESSURE_HISTORY_LENGTH)
                keyState->ehPressureHistory.pop_front();

            ConfigLookup::Key& keyLookup = configLookups_[deviceIndex].keys[oscMsg.course][oscMsg.key];
            if (keyLookup.output == MidiChannelType::Undefined) {
                if (oscMsg.active)
                     juce::Logger::writeToLog("MidiService: Key press on undefined mapping - Course: " + juce::String(oscMsg.course) + ", Key: " + juce::String(oscMsg.key) + " for device " + juce::String(deviceIndex + 1));
                break;
            }
            
            if (keyLookup.mapType == KeyMappingType::Note || keyLookup.mapType == KeyMappingType::Chord)
                processNoteKey(oscMsg, keyLookup, keyState, midiBuffer);
            else if (keyLookup.mapType == KeyMappingType::MidiMsg)
                processCmdKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer);
            break;
        }
        case osc::MessageType::Breath: {
            float prevBreathValue = ehBreath_[deviceIndex];
            ehBreath_[deviceIndex] = std::abs(oscMsg.value);
            if ((ehBreath_[deviceIndex] > breathZeroThreshold_[deviceIndex]) || 
                (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex] && prevBreathValue > 0.0f)) {
                createBreath(deviceIndex, configLookups_[deviceIndex], midiBuffer);
            }
            break;
        }
        case osc::MessageType::Strip: {
            int stripIndex = static_cast<int>(oscMsg.strip) - 1;
            if (stripIndex < 0 || stripIndex > 1) break;
            
            bool stripOff = !oscMsg.active;
            ehStrips_[stripIndex][deviceIndex] = stripOff ? 0.0f : std::max((oscMsg.value - stripZeroThreshold_[deviceIndex]) * stripGain_[deviceIndex], 0.0f);
            
            if (stripOff) {
                relStart_ehStrips_[stripIndex][deviceIndex] = -1.0f;
            } else if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
                relStart_ehStrips_[stripIndex][deviceIndex] = ehStrips_[stripIndex][deviceIndex];
            }

            for (int i = 0; i < 3; i++) {
                if (!stripOff) {
                    createStripAbsolute(deviceIndex, stripIndex, i, configLookups_[deviceIndex], midiBuffer);
                }
                createStripRelative(deviceIndex, stripIndex, i, configLookups_[deviceIndex], midiBuffer);
            }
            break;
        }
        default: break;
    }

    if (!configLookups_[deviceIndex].controlLights && outgoingOscMsg.type == osc::MessageType::LED)
        outgoingOscMsg.type = osc::MessageType::Undefined;
}

void MidiService::processNoteKey(osc::Message& oscMsg, ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer) {
    state->messageCount++;

    if (!oscMsg.active) {
        createNoteOff(keyLookup, state, buffer);
    } else if (state->status == KeyStatus::Off) {
        state->status = KeyStatus::Pending;
    } else if (state->messageCount == PRESSURE_HISTORY_LENGTH && state->status == KeyStatus::Pending) {
        createNoteOn(keyLookup, state, buffer);
    } else if (state->status == KeyStatus::Active) {
        createNoteHold(keyLookup, state, buffer);
    }
}

void MidiService::processCmdKey(osc::Message& oscMsg, osc::Message& outgoingOscMsg, ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer) {
    if (!oscMsg.active) {
        if (keyLookup.cmdType == 2) // Momentary
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg);
    } else if (state->status == KeyStatus::Off) {
        if (keyLookup.cmdType == 1 && state->isLatchOn)
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg);
        else
            createMidiMsgOn(keyLookup, state, buffer, outgoingOscMsg);
    }
    state->status = oscMsg.active ? KeyStatus::Active : KeyStatus::Off;
}

void MidiService::reduceBreath(juce::MidiBuffer& buffer) {
    for (int i = 0; i < 3; i++) {
        const juce::ScopedLock sl(configLookups_[i].getLock());
        if (ehBreath_[i] <= 0.0f) continue;
        ehBreath_[i] = (ehBreath_[i] > breathZeroThreshold_[i]) ? ehBreath_[i] - 0.005f : 0.0f;
        createBreath(i, configLookups_[i], buffer);
    }
}

void MidiService::createBreath(int deviceIndex, ConfigLookup& keyLookup, juce::MidiBuffer& buffer) {
    float val = (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex]) 
                       ? 0.0f : ehBreath_[deviceIndex] - breathZeroThreshold_[deviceIndex];
    
    // Scale val to [0, 1] range after threshold
    if (val > 0.0f) {
        val = val / (1.0f - breathZeroThreshold_[deviceIndex]);
    }

    for (int z = 0; z < 3; ++z) {
        addMidiValueMessage(keyLookup.breath[z].channel, val * 3.0f, keyLookup.breath[z].midiValue, 1.0f, 0, buffer, false);
    }
}

void MidiService::createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup& keyLookup, juce::MidiBuffer& buffer) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    addStripValueMessage(strip.channel, ehStrips_[stripIndex][deviceIndex], strip.absMidiValue, buffer, false);
}

void MidiService::createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup& keyLookup, juce::MidiBuffer& buffer) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    float relValue = (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) 
                   ? 0.0f : relStart_ehStrips_[stripIndex][deviceIndex] - ehStrips_[stripIndex][deviceIndex];
    
    if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
        currentStripPBperChannel_[strip.channel > 0 ? strip.channel - 1 : 0] = 0;
    }

    addStripValueMessage(strip.channel, relValue, strip.relMidiValue, buffer, true);
}

void MidiService::createNoteOn(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer) {
    if (keyLookup.output == MidiChannelType::MPE_Low && lowerChanAssigner_)
        state->midiChannel = lowerChanAssigner_->findMidiChannelForNewNote(keyLookup.notes[0]);
    else if (keyLookup.output == MidiChannelType::MPE_High && upperChanAssigner_)
        state->midiChannel = upperChanAssigner_->findMidiChannelForNewNote(keyLookup.notes[0]);
    else
        state->midiChannel = static_cast<int>(keyLookup.output);

    if (state->midiChannel > 0 && state->midiChannel <= 16)
        chanNotePri_[state->midiChannel - 1].push_front(keyLookup.keyId);

    createNoteHold(keyLookup, state, buffer);
    auto vel = calculateNoteOnVelocity(state);
    int eventTime = 0;
    
    for (int i = 0; i < 4; i++) {
        if (keyLookup.notes[i] > -1) {
            if (countPlayingNoteMatches(state->midiChannel, keyLookup.notes[i]) == 0) {
                buffer.addEvent(juce::MidiMessage::noteOn(state->midiChannel, keyLookup.notes[i], vel.asUnsignedFloat()), eventTime);
            }
            playingNotes_.push_back({state->midiChannel, keyLookup.notes[i]});
        }
    }
    state->status = KeyStatus::Active;
}

void MidiService::createNoteOff(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer) {
    int channel = state->midiChannel;
    if (keyLookup.output == MidiChannelType::MPE_Low && lowerChanAssigner_)
        lowerChanAssigner_->noteOff(keyLookup.notes[0], channel);
    else if (keyLookup.output == MidiChannelType::MPE_High && upperChanAssigner_)
        upperChanAssigner_->noteOff(keyLookup.notes[0], channel);

    if (channel > 0 && channel <= 16) {
        chanNotePri_[channel - 1].remove_if([&keyLookup](const LayoutWrapper::KeyId& id) { return id == keyLookup.keyId; });
    }

    int eventTime = 0;
    auto vel = calculateNoteOffVelocity(state);
    for (int i = 0; i < 4; i++) {
        if (keyLookup.notes[i] > -1) {
            if (countPlayingNoteMatches(channel, keyLookup.notes[i]) < 2) {
                buffer.addEvent(juce::MidiMessage::noteOff(channel, keyLookup.notes[i], vel.asUnsignedFloat()), eventTime);
            }
            removeOneNoteMatch(channel, keyLookup.notes[i]);
        }
    }
    
    if (channel > 0 && channel <= 16 && chanNotePri_[channel - 1].empty()) {
        addMidiValueMessage(channel, 0, keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], buffer, false);
        addMidiValueMessage(channel, 0, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
        addMidiValueMessage(channel, 0, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
    }
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiService::createMidiMsgOn(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg) {
    state->isLatchOn = true;
    state->midiChannel = (keyLookup.output == MidiChannelType::MPE_Low) ? 1 : 
                         (keyLookup.output == MidiChannelType::MPE_High) ? 16 : static_cast<int>(keyLookup.output);

    int eventTime = 0;
    if (keyLookup.msgType == 4) {
        createAllNotesOff(buffer);
    } else if (keyLookup.msgType == 1) {
        buffer.addEvent(juce::MidiMessage::controllerEvent(state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOn), eventTime);
    } else if (keyLookup.msgType == 2) {
        buffer.addEvent(juce::MidiMessage::programChange(state->midiChannel, keyLookup.cmdOn), eventTime);
    } else if (keyLookup.msgType == 3) {
        if (keyLookup.cmdOn == 1) buffer.addEvent(juce::MidiMessage::midiStart(), eventTime);
        else if (keyLookup.cmdOn == 2) buffer.addEvent(juce::MidiMessage::midiStop(), eventTime);
        else if (keyLookup.cmdOn == 3) buffer.addEvent(juce::MidiMessage::midiContinue(), eventTime);
    }
    
    state->status = KeyStatus::Active;
    if (keyLookup.cmdType == 1) {
        outgoingOscMsg.type = osc::MessageType::LED;
        outgoingOscMsg.device = keyLookup.keyId.deviceType;
        outgoingOscMsg.course = keyLookup.keyId.course;
        outgoingOscMsg.key = keyLookup.keyId.keyNo;
        outgoingOscMsg.value = static_cast<unsigned int>(KeyColour::Yellow);
        if (oscBroadcastQueue_) oscBroadcastQueue_->add(outgoingOscMsg);
    }
}

void MidiService::createMidiMsgOff(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg) {
    if (keyLookup.cmdType != 3) { // Not Trigger
        int eventTime = buffer.getLastEventTime() + 1;
        if (keyLookup.msgType == 4) {
            createAllNotesOff(buffer);
        } else if (keyLookup.msgType == 1) {
            buffer.addEvent(juce::MidiMessage::controllerEvent(state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOff), eventTime);
        } else if (keyLookup.msgType == 2) {
            buffer.addEvent(juce::MidiMessage::programChange(state->midiChannel, keyLookup.cmdOff), eventTime);
        } else if (keyLookup.msgType == 3) {
            if (keyLookup.cmdOff == 1) buffer.addEvent(juce::MidiMessage::midiStart(), eventTime);
            else if (keyLookup.cmdOff == 2) buffer.addEvent(juce::MidiMessage::midiStop(), eventTime);
            else if (keyLookup.cmdOff == 3) buffer.addEvent(juce::MidiMessage::midiContinue(), eventTime);
        }
    }
    
    state->status = KeyStatus::Off;
    state->isLatchOn = false;
    if (keyLookup.cmdType == 1) {
        outgoingOscMsg.type = osc::MessageType::LED;
        outgoingOscMsg.device = keyLookup.keyId.deviceType;
        outgoingOscMsg.course = keyLookup.keyId.course;
        outgoingOscMsg.key = keyLookup.keyId.keyNo;
        outgoingOscMsg.value = static_cast<unsigned int>(keyLookup.keyColour);
        if (oscBroadcastQueue_) oscBroadcastQueue_->add(outgoingOscMsg);
    }
}

void MidiService::createAllNotesOff(juce::MidiBuffer& buffer) {
    for (int i = 1; i <= 16; i++) {
        buffer.addEvent(juce::MidiMessage::allNotesOff(i), 0);
        chanNotePri_[i - 1].clear();
    }
    if (lowerChanAssigner_) lowerChanAssigner_->allNotesOff();
    if (upperChanAssigner_) upperChanAssigner_->allNotesOff();
    playingNotes_.clear();
}

void MidiService::createNoteHold(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer) {
    int channel = state->midiChannel;
    if (channel > 0 && channel <= 16 && (chanNotePri_[channel - 1].empty() || chanNotePri_[channel - 1].front() == keyLookup.keyId)) {
        addMidiValueMessage(channel, state->ehRoll, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
        addMidiValueMessage(channel, state->ehYaw, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], buffer, true);
        addMidiValueMessage(channel, state->ehPressureHistory.back(), keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], buffer, false);
    }
    state->messageCount = 0;
}

void MidiService::addMidiValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer& buffer, bool isBipolar) {
    if (midiValue.valueType == MidiValueType::Off || channel < 1 || channel > 16) return;
    
    float gain = 1.7f;
    if (!isBipolar && midiValue.valueType == MidiValueType::CC) gain = 1.0f;
    float normalized = isBipolar ? (std::clamp(ehValue * 1.7f, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    
    juce::MidiMessage msg;
    if (midiValue.valueType == MidiValueType::Pitchbend) {
        currentKeyPBperChannel_[channel - 1] = static_cast<int>(calculatePitchBendCurve(normalized) * pbRange * 8191.0f);
        int totalPB = std::clamp(currentKeyPBperChannel_[channel - 1] + currentStripPBperChannel_[channel - 1] + 8192, 0, 16383);
        msg = juce::MidiMessage::pitchWheel(channel, totalPB);
    } else if (midiValue.valueType == MidiValueType::ChannelAftertouch) {
        int at = isBipolar ? static_cast<int>(normalized * 63 + 64) : static_cast<int>(normalized * 127);
        msg = juce::MidiMessage::channelPressureChange(channel, std::clamp(at, 0, 127));
    } else if (midiValue.valueType == MidiValueType::PolyAftertouch) {
        int at = isBipolar ? static_cast<int>(normalized * 63 + 64) : static_cast<int>(normalized * 127);
        msg = juce::MidiMessage::aftertouchChange(channel, noteNo, std::clamp(at, 0, 127));
    } else if (midiValue.valueType == MidiValueType::CC) {
        int cc = isBipolar ? static_cast<int>(normalized * 63 + 64) : static_cast<int>(normalized * 127);
        msg = juce::MidiMessage::controllerEvent(channel, midiValue.ccNo, std::clamp(cc, 0, 127));
    }
    
    if (msg.getRawDataSize() > 0)
        buffer.addEvent(msg, 0);
}

void MidiService::addStripValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer& buffer, bool isBipolar) {
    if (midiValue.valueType == MidiValueType::Off || channel < 1 || channel > 16) return;
    
    float gain = isBipolar ? 1.7f : 1.0f;
    float normalized = isBipolar ? (std::clamp(ehValue * gain, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    
    juce::MidiMessage msg;
    if (midiValue.valueType == MidiValueType::Pitchbend) {
        currentStripPBperChannel_[channel - 1] = static_cast<int>((isBipolar ? calculatePitchBendCurve(normalized) : normalized) * 8191.0f);
        int totalPB = std::clamp(currentKeyPBperChannel_[channel - 1] + currentStripPBperChannel_[channel - 1] + 8192, 0, 16383);
        msg = juce::MidiMessage::pitchWheel(channel, totalPB);
    } else {
        int val = isBipolar ? static_cast<int>(normalized * 63 + 64) : static_cast<int>(normalized * 127);
        if (midiValue.valueType == MidiValueType::ChannelAftertouch)
            msg = juce::MidiMessage::channelPressureChange(channel, std::clamp(val, 0, 127));
        else if (midiValue.valueType == MidiValueType::CC)
            msg = juce::MidiMessage::controllerEvent(channel, midiValue.ccNo, std::clamp(val, 0, 127));
    }

    if (msg.getRawDataSize() > 0)
        buffer.addEvent(msg, 0);
}

void MidiService::createLayoutRPNs(juce::MidiBuffer& buffer) {
    buffer.clear();
    auto buff = juce::MPEMessages::setZoneLayout(mpeZone_);
    buffer.addEvents(buff, 0, -1, 0);
}

float MidiService::calculatePitchBendCurve(float value) const {
    return std::clamp(std::tan(value) / 3.14159265f * 2.0f, -1.0f, 1.0f);
}

juce::MPEValue MidiService::calculateNoteOnVelocity(KeyState* state) {
    if (state->ehPressureHistory.size() < PRESSURE_HISTORY_LENGTH) return juce::MPEValue::from7BitInt(1);
    
    auto it = state->ehPressureHistory.begin();
    float v1 = (it[1] + it[2]) / 2.0f;
    float v2 = (it[4] + it[5]) / 2.0f;
    float diff = v2 - v1;
    // We expect diff to be in [0, 1] range roughly. Bezier curve table is 256 entries.
    int tableIndex = std::clamp(static_cast<int>(diff * 4096.0f), 0, BezierCurve::TABLE_LENGTH - 1);
    return juce::MPEValue::from7BitInt(static_cast<int>(velocityCurve_.getTableValue(tableIndex) * 126 + 1));
}

juce::MPEValue MidiService::calculateNoteOffVelocity(KeyState* state) {
    if (state->ehPressureHistory.empty()) return juce::MPEValue::from7BitInt(0);
    float norm = std::min(state->ehPressureHistory.front() * 10.0f, 1.0f);
    return juce::MPEValue::from7BitInt(static_cast<int>(norm * 127));
}

int MidiService::countPlayingNoteMatches(int channel, int noteNumber) const {
    int count = 0;
    for (const auto& n : playingNotes_) {
        if (n.channel == channel && n.noteNumber == noteNumber) count++;
    }
    return count;
}

void MidiService::removeOneNoteMatch(int channel, int noteNumber) {
    for (auto it = playingNotes_.begin(); it != playingNotes_.end(); ++it) {
        if (it->channel == channel && it->noteNumber == noteNumber) {
            playingNotes_.erase(it);
            break;
        }
    }
}

} // namespace ecm
