#include "MidiService.h"
#include "HardwareService.h"
#include <cmath>

namespace ecm {

MidiService::MidiService(ConfigLookup (&configLookups)[3], juce::CriticalSection& stateLock)
    : configLookups_(configLookups), stateLock_(stateLock) {
}

MidiService::~MidiService() {
    stop();
}

void MidiService::start(juce::AudioProcessorValueTreeState& pluginState, HardwareService* hs) {
    const juce::ScopedLock stateGuard(stateLock_);
    hardwareService_ = hs;
    pluginState_ = &pluginState;
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
        latchTranspose_[i] = 0;
        momentaryTranspose_[i] = 0;
        for (int c = 0; c < 3; ++c) {
            for (int k = 0; k < 120; ++k) {
                keyStates_[i][c][k].isLatchOn = false;
                keyStates_[i][c][k].status = KeyStatus::Off;
                for (int n = 0; n < 4; ++n)
                    keyStates_[i][c][k].activeNotes[n] = -1;
            }
        }
    }

    ehBreath_[0] = ehBreath_[1] = ehBreath_[2] = 0.0f;
    for (int i = 0; i < 3; ++i) {
        for (int t = 0; t < 6; ++t) {
            recentVisualEvents_[i][t].clear();
        }
    }
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
    pluginState_ = nullptr;
    {
        const juce::ScopedLock sl(pendingMessageLock_);
        pendingMidiMessages_.clear();
    }
}

void MidiService::setRuntimeConfigSnapshot(std::shared_ptr<RuntimeConfigSnapshot> snapshot)
{
    runtimeConfigSnapshot_.store(std::move(snapshot), std::memory_order_release);
}

void MidiService::processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, int eventTime, int* presetSlotRequest) {
    if (!initialized_) return;

    auto snapshot = runtimeConfigSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    const auto& runtimeLookups = snapshot->configLookups;
    
    std::strncpy(outgoingOscMsg.devId, oscMsg.devId, 63);
    outgoingOscMsg.device = oscMsg.device;

    int deviceIndex = static_cast<int>(oscMsg.device) - 1;
    jassert(deviceIndex >= 0 && deviceIndex < 3);
    if (deviceIndex < 0 || deviceIndex > 2) {
        if (oscMsg.type == osc::MessageType::Key && oscMsg.active)
             juce::Logger::writeToLog("MidiService: Invalid device index: " + juce::String(deviceIndex + 1));
        return;
    }

    const auto& deviceLookups = runtimeLookups[(size_t)deviceIndex];

    switch (oscMsg.type) {
        case osc::MessageType::Key: {
            if (oscMsg.course >= 3 || oscMsg.key >= 120) break;
            KeyState* keyState = &keyStates_[deviceIndex][oscMsg.course][oscMsg.key];
            keyState->ehYaw = oscMsg.yaw;
            keyState->ehRoll = oscMsg.roll;
            keyState->ehPressureHistory.push_back(oscMsg.pressure);
            while (keyState->ehPressureHistory.size() > PRESSURE_HISTORY_LENGTH)
                keyState->ehPressureHistory.pop_front();

            const ConfigLookup::Key& keyLookup = deviceLookups.keys[oscMsg.course][oscMsg.key];
            if (keyLookup.output == MidiChannelType::Undefined) {
                if (oscMsg.active)
                     juce::Logger::writeToLog("MidiService: Key press on undefined mapping - Course: " + juce::String(oscMsg.course) + ", Key: " + juce::String(oscMsg.key) + " for device " + juce::String(deviceIndex + 1));
                break;
            }
            
            if (keyLookup.mapType == KeyMappingType::Note || keyLookup.mapType == KeyMappingType::Chord)
                processNoteKey(oscMsg, keyLookup, keyState, midiBuffer, eventTime);
            else if (keyLookup.mapType == KeyMappingType::MidiMsg)
                processCmdKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer, eventTime);
            else if (keyLookup.mapType == KeyMappingType::AppCtrl)
                processAppCtrlKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer, eventTime, presetSlotRequest);
            break;
        }
        case osc::MessageType::Breath: {
            float prevBreathValue = ehBreath_[deviceIndex];
            ehBreath_[deviceIndex] = std::abs(oscMsg.value);
            if ((ehBreath_[deviceIndex] > breathZeroThreshold_[deviceIndex]) || 
                (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex] && prevBreathValue > 0.0f)) {
                createBreath(deviceIndex, deviceLookups, midiBuffer, eventTime);
            }
            break;
        }
        case osc::MessageType::Strip: {
            const int stripIndex = static_cast<int>(oscMsg.strip) - 1;
            if (stripIndex < 0 || stripIndex > 1) break;

            const bool stripOff = !oscMsg.active;
            ehStrips_[stripIndex][deviceIndex] = stripOff ? 0.0f : std::max((oscMsg.value - stripZeroThreshold_[deviceIndex]) * stripGain_[deviceIndex], 0.0f);
            
            if (stripOff) {
                relStart_ehStrips_[stripIndex][deviceIndex] = -1.0f;
            } else if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
                relStart_ehStrips_[stripIndex][deviceIndex] = ehStrips_[stripIndex][deviceIndex];
            }

            for (int i = 0; i < 3; i++) {
                if (!stripOff) {
                    createStripAbsolute(deviceIndex, stripIndex, i, deviceLookups, midiBuffer, eventTime);
                }
                createStripRelative(deviceIndex, stripIndex, i, deviceLookups, midiBuffer, eventTime);
            }
            break;
        }
        default: break;
    }

    bool controlLights = deviceLookups.controlLights;
    if (controlLights && hardwareService_ && std::strlen(outgoingOscMsg.devId) > 0) {
        controlLights = hardwareService_->isDeviceAuthorizedForLEDs(outgoingOscMsg.devId);
    }

    if (!controlLights && outgoingOscMsg.type == osc::MessageType::LED)
        outgoingOscMsg.type = osc::MessageType::Undefined;
}

void MidiService::processNoteKey(const osc::Message& oscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime) {
    state->messageCount++;

    if (!oscMsg.active) {
        createNoteOff(keyLookup, state, buffer, eventTime);
    } else if (state->status == KeyStatus::Off) {
        state->status = KeyStatus::Pending;
    } else if (state->messageCount == PRESSURE_HISTORY_LENGTH && state->status == KeyStatus::Pending) {
        createNoteOn(keyLookup, state, buffer, eventTime);
    } else if (state->status == KeyStatus::Active) {
        createNoteHold(keyLookup, state, buffer, eventTime);
    }
}

void MidiService::processCmdKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime) {
    if (!oscMsg.active) {
        if (keyLookup.cmdType == 2) // Momentary
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg, oscMsg.devId, eventTime);
    } else if (state->status == KeyStatus::Off) {
        if (keyLookup.cmdType == 1 && state->isLatchOn)
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg, oscMsg.devId, eventTime);
        else
            createMidiMsgOn(keyLookup, state, buffer, outgoingOscMsg, oscMsg.devId, eventTime);
    }
    state->status = oscMsg.active ? KeyStatus::Active : KeyStatus::Off;
}

void MidiService::processAppCtrlKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, int* presetSlotRequest) {
    int deviceIndex = static_cast<int>(keyLookup.keyId.deviceType) - 1;
    if (deviceIndex < 0 || deviceIndex > 2) return;

    if (keyLookup.appCtrlType == 1) { // Preset
        if (oscMsg.active && state->status == KeyStatus::Off) {
            if (presetSlotRequest != nullptr)
                *presetSlotRequest = keyLookup.appCtrlValue;
        }
    } else if (keyLookup.appCtrlType == 2) { // Transpose
        int mode = keyLookup.cmdType; // 1 = Latch, 2 = Momentary, 3 = Trigger
        int value = keyLookup.appCtrlValue;

        if (mode == 1) { // Latch
            if (oscMsg.active && state->status == KeyStatus::Off) {
                if (state->isLatchOn) {
                    state->isLatchOn = false;
                    latchTranspose_[deviceIndex] = 0;
                } else {
                    clearAllAppCtrlTransposes(deviceIndex);
                    state->isLatchOn = true;
                    latchTranspose_[deviceIndex] = value;
                }
                
                // Immediate feedback for the current key
                outgoingOscMsg.type = osc::MessageType::LED;
                outgoingOscMsg.device = keyLookup.keyId.deviceType;
                std::strncpy(outgoingOscMsg.devId, oscMsg.devId, 63);
                outgoingOscMsg.course = keyLookup.keyId.course;
                outgoingOscMsg.key = keyLookup.keyId.keyNo;
                outgoingOscMsg.value = state->isLatchOn ? static_cast<float>(KeyColour::Yellow) : static_cast<float>(keyLookup.keyColour);

                resendLEDs(oscMsg.devId, keyLookup.keyId.deviceType);
            }
        } else if (mode == 2) { // Momentary
            if (oscMsg.active && state->status == KeyStatus::Off) {
                momentaryTranspose_[deviceIndex] += value;
            } else if (!oscMsg.active && state->status != KeyStatus::Off) {
                momentaryTranspose_[deviceIndex] -= value;
            }
        } else if (mode == 3) { // Trigger
            if (oscMsg.active && state->status == KeyStatus::Off) {
                clearAllAppCtrlTransposes(deviceIndex);
                latchTranspose_[deviceIndex] = value;
                resendLEDs(oscMsg.devId, keyLookup.keyId.deviceType);
            }
        }
    }
    state->status = oscMsg.active ? KeyStatus::Active : KeyStatus::Off;
}

void MidiService::clearAllAppCtrlTransposes(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex > 2) return;
    
    latchTranspose_[deviceIndex] = 0;
    for (int c = 0; c < 3; ++c) {
        for (int k = 0; k < 120; ++k) {
            auto& keyLookup = configLookups_[deviceIndex].keys[c][k];
            if (keyLookup.mapType == KeyMappingType::AppCtrl && keyLookup.appCtrlType == 2 && keyLookup.cmdType == 1) {
                keyStates_[deviceIndex][c][k].isLatchOn = false;
            }
        }
    }
}

void MidiService::handleRemotePerformanceData(osc::Message& oscMsg, juce::MidiBuffer& midiBuffer, int eventTime) {
    osc::Message outgoingMsg;
    processMessage(oscMsg, outgoingMsg, midiBuffer, eventTime, nullptr);
}

void MidiService::drainPendingMidiMessages(juce::MidiBuffer& buffer, int eventTime) {
    std::vector<PendingMidiMessage> messagesToDrain;
    {
        const juce::ScopedLock sl(pendingMessageLock_);
        messagesToDrain.swap(pendingMidiMessages_);
    }

    for (const auto& pending : messagesToDrain)
        buffer.addEvent(pending.message, pending.eventTime >= 0 ? pending.eventTime : eventTime);
}

void MidiService::resendLEDs(const char* devId, InstrumentType type, osc::MessageFifo* targetQueue, bool onlyNonOff) {
    if (!initialized_) return;
    const juce::ScopedLock stateGuard(stateLock_);
    int deviceIndex = static_cast<int>(type) - 1;
    if (deviceIndex < 0 || deviceIndex > 2) return;
    
    if (!configLookups_[deviceIndex].controlLights) return;
    if (hardwareService_ && !hardwareService_->isDeviceAuthorizedForLEDs(devId)) return;

    osc::MessageFifo* queue = targetQueue;
    if (!queue) {
        if (hardwareService_ && hardwareService_->getDeviceMode(devId) == ecm::DeviceMode::Local)
            queue = localHardwareQueue_;
        else
            queue = oscBroadcastQueue_;
    }
    
    if (!queue) return;
    
    const juce::ScopedLock sl(configLookups_[deviceIndex].getLock());
    for (int course = 0; course < 3; ++course) {
        for (int keyNo = 0; keyNo < 120; ++keyNo) {
            auto& keyLookup = configLookups_[deviceIndex].keys[course][keyNo];
            
            unsigned int colour = (unsigned int)KeyColour::Off;
            
            if (keyLookup.mapType == KeyMappingType::MidiMsg && keyLookup.cmdType == 1) {
                // Command Latch
                if (keyStates_[deviceIndex][course][keyNo].isLatchOn) {
                    colour = (unsigned int)KeyColour::Yellow;
                } else {
                    colour = (unsigned int)keyLookup.keyColour;
                }
            } else if (keyLookup.mapType == KeyMappingType::AppCtrl && keyLookup.appCtrlType == 2 && keyLookup.cmdType == 1) {
                // App Ctrl Transpose Latch
                if (keyStates_[deviceIndex][course][keyNo].isLatchOn) {
                    colour = (unsigned int)KeyColour::Yellow;
                } else {
                    colour = (unsigned int)keyLookup.keyColour;
                }
            } else if (keyLookup.mapType != KeyMappingType::None) {
                colour = (unsigned int)keyLookup.keyColour;
            }
            
            if (onlyNonOff && colour == (unsigned int)KeyColour::Off) continue;
            
            osc::Message outgoingMsg;
            outgoingMsg.type = osc::MessageType::LED;
            outgoingMsg.device = type;
            std::strncpy(outgoingMsg.devId, devId, 63);
            outgoingMsg.course = (unsigned int)course;
            outgoingMsg.key = (unsigned int)keyNo;
            outgoingMsg.value = (float)colour;
            
            queue->add(outgoingMsg);
        }
    }
}

void MidiService::reduceBreath(juce::MidiBuffer& buffer, int eventTime) {
    auto snapshot = runtimeConfigSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    const auto& runtimeLookups = snapshot->configLookups;
    for (int i = 0; i < 3; i++) {
        if (ehBreath_[i] <= 0.0f) continue;
        ehBreath_[i] = (ehBreath_[i] > breathZeroThreshold_[i]) ? ehBreath_[i] - 0.005f : 0.0f;
        createBreath(i, runtimeLookups[i], buffer, eventTime);
    }
}

void MidiService::createBreath(int deviceIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime) {
    float val = (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex]) 
                       ? 0.0f : ehBreath_[deviceIndex] - breathZeroThreshold_[deviceIndex];
    
    // Scale val to [0, 1] range after threshold
    if (val > 0.0f) {
        val = val / (1.0f - breathZeroThreshold_[deviceIndex]);
    }

    for (int z = 0; z < 3; ++z) {
        addMidiValueMessage(static_cast<InstrumentType>(deviceIndex + 1), keyLookup.breath[z].channel, val * 3.0f, keyLookup.breath[z].midiValue, 1.0f, 0, buffer, false, ExpressionCurveTarget::Breath, eventTime);
    }
}

void MidiService::createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    addStripValueMessage(strip.channel, ehStrips_[stripIndex][deviceIndex], strip.absMidiValue, buffer, false, eventTime);
}

void MidiService::createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    float relValue = (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) 
                   ? 0.0f : relStart_ehStrips_[stripIndex][deviceIndex] - ehStrips_[stripIndex][deviceIndex];
    
    if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
        currentStripPBperChannel_[strip.channel > 0 ? strip.channel - 1 : 0] = 0;
    }

    addStripValueMessage(strip.channel, relValue, strip.relMidiValue, buffer, true, eventTime);
}

void MidiService::createNoteOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime) {
    int deviceIndex = static_cast<int>(keyLookup.keyId.deviceType) - 1;
    int totalTranspose = (deviceIndex >= 0 && deviceIndex < 3) ? (latchTranspose_[deviceIndex] + momentaryTranspose_[deviceIndex]) : 0;

    if (keyLookup.output == MidiChannelType::MPE_Low && lowerChanAssigner_)
        state->midiChannel = lowerChanAssigner_->findMidiChannelForNewNote(keyLookup.notes[0]);
    else if (keyLookup.output == MidiChannelType::MPE_High && upperChanAssigner_)
        state->midiChannel = upperChanAssigner_->findMidiChannelForNewNote(keyLookup.notes[0]);
    else
        state->midiChannel = static_cast<int>(keyLookup.output);

    if (state->midiChannel > 0 && state->midiChannel <= 16)
        chanNotePri_[state->midiChannel - 1].push_front(keyLookup.keyId);

    // Prepare activeNotes BEFORE calling createNoteHold if it depends on them, 
    // but createNoteHold usually just sends expression data.
    for (int i = 0; i < 4; i++) {
        if (keyLookup.notes[i] > -1) {
            state->activeNotes[i] = std::clamp(keyLookup.notes[i] + totalTranspose, 0, 127);
        } else {
            state->activeNotes[i] = -1;
        }
    }

    createNoteHold(keyLookup, state, buffer, eventTime);
    auto vel = calculateNoteOnVelocity(keyLookup.keyId.deviceType, state);
    
    for (int i = 0; i < 4; i++) {
        int noteNo = state->activeNotes[i];
        if (noteNo > -1) {
            if (countPlayingNoteMatches(state->midiChannel, noteNo) == 0) {
                buffer.addEvent(juce::MidiMessage::noteOn(state->midiChannel, noteNo, vel.asUnsignedFloat()), eventTime);
            }
            playingNotes_.push_back({state->midiChannel, noteNo});
        }
    }
    state->status = KeyStatus::Active;
}

void MidiService::createNoteOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime) {
    int channel = state->midiChannel;
    if (keyLookup.output == MidiChannelType::MPE_Low && lowerChanAssigner_)
        lowerChanAssigner_->noteOff(keyLookup.notes[0], channel);
    else if (keyLookup.output == MidiChannelType::MPE_High && upperChanAssigner_)
        upperChanAssigner_->noteOff(keyLookup.notes[0], channel);

    if (channel > 0 && channel <= 16) {
        chanNotePri_[channel - 1].remove_if([&keyLookup](const LayoutWrapper::KeyId& id) { return id == keyLookup.keyId; });
    }

    auto vel = calculateNoteOffVelocity(keyLookup.keyId.deviceType, state);
    for (int i = 0; i < 4; i++) {
        int noteToTurnOff = state->activeNotes[i];
        if (noteToTurnOff > -1) {
            if (countPlayingNoteMatches(channel, noteToTurnOff) < 2) {
                buffer.addEvent(juce::MidiMessage::noteOff(channel, noteToTurnOff, vel.asUnsignedFloat()), eventTime);
            }
            removeOneNoteMatch(channel, noteToTurnOff);
            state->activeNotes[i] = -1;
        }
    }
    
    if (channel > 0 && channel <= 16 && chanNotePri_[channel - 1].empty()) {
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], buffer, false, ExpressionCurveTarget::Pressure, eventTime);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], buffer, true, ExpressionCurveTarget::Roll, eventTime);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], buffer, true, ExpressionCurveTarget::Yaw, eventTime);
    }
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiService::createMidiMsgOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime) {
    state->isLatchOn = true;
    state->midiChannel = (keyLookup.output == MidiChannelType::MPE_Low) ? 1 : 
                         (keyLookup.output == MidiChannelType::MPE_High) ? 16 : static_cast<int>(keyLookup.output);

    if (keyLookup.msgType == 4) {
        createAllNotesOff(buffer, eventTime);
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
        std::strncpy(outgoingOscMsg.devId, devId, 63);
        outgoingOscMsg.course = keyLookup.keyId.course;
        outgoingOscMsg.key = keyLookup.keyId.keyNo;
        outgoingOscMsg.value = static_cast<unsigned int>(KeyColour::Yellow);
    }
}

void MidiService::createMidiMsgOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime) {
    if (keyLookup.cmdType != 3) { // Not Trigger
        if (keyLookup.msgType == 4) {
            createAllNotesOff(buffer, eventTime);
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
        std::strncpy(outgoingOscMsg.devId, devId, 63);
        outgoingOscMsg.course = keyLookup.keyId.course;
        outgoingOscMsg.key = keyLookup.keyId.keyNo;
        outgoingOscMsg.value = static_cast<unsigned int>(keyLookup.keyColour);
    }
}

void MidiService::createAllNotesOff(juce::MidiBuffer& buffer, int eventTime) {
    for (int i = 1; i <= 16; i++) {
        buffer.addEvent(juce::MidiMessage::allNotesOff(i), eventTime);
        chanNotePri_[i - 1].clear();
    }
    if (lowerChanAssigner_) lowerChanAssigner_->allNotesOff();
    if (upperChanAssigner_) upperChanAssigner_->allNotesOff();
    playingNotes_.clear();
}

void MidiService::appendPendingMidiMessage(const juce::MidiMessage& message, int eventTime) {
    const juce::ScopedLock sl(pendingMessageLock_);
    pendingMidiMessages_.push_back({message, eventTime});
}

void MidiService::queueTransposeChangeFlush(InstrumentType deviceType, Zone zone) {
    if (!initialized_ || pluginState_ == nullptr || deviceType == InstrumentType::None || zone == Zone::NoZone)
        return;

    const juce::ScopedLock stateGuard(stateLock_);

    int deviceIndex = static_cast<int>(deviceType) - 1;
    jassert(deviceIndex >= 0 && deviceIndex < 3);
    if (deviceIndex < 0 || deviceIndex > 2)
        return;

    std::vector<PendingMidiMessage> localMessages;

    const juce::ScopedLock sl(configLookups_[deviceIndex].getLock());
    auto& state = pluginState_->state;

    for (int course = 0; course < 3; ++course) {
        for (int keyNo = 0; keyNo < 120; ++keyNo) {
            LayoutWrapper::KeyId keyId { course, keyNo, deviceType };
            auto layoutKey = LayoutWrapper::getLayoutKey(keyId, state);
            if (layoutKey.zone != zone)
                continue;

            auto& keyState = keyStates_[deviceIndex][course][keyNo];
            auto& keyLookup = configLookups_[deviceIndex].keys[course][keyNo];

            if (keyState.status == KeyStatus::Active) {
                int channel = keyState.midiChannel;
                auto vel = calculateNoteOffVelocity(deviceType, &keyState);

                for (int i = 0; i < 4; ++i) {
                    if (keyLookup.notes[i] > -1) {
                        if (countPlayingNoteMatches(channel, keyLookup.notes[i]) < 2)
                            localMessages.push_back({ juce::MidiMessage::noteOff(channel, keyLookup.notes[i], vel.asUnsignedFloat()), 0 });
                        removeOneNoteMatch(channel, keyLookup.notes[i]);
                    }
                }

                if (channel > 0 && channel <= 16) {
                    chanNotePri_[channel - 1].remove_if([&keyId](const LayoutWrapper::KeyId& id) { return id == keyId; });

                    if (chanNotePri_[channel - 1].empty()) {
                        currentKeyPBperChannel_[channel - 1] = 0;
                        currentStripPBperChannel_[channel - 1] = 0;
                        localMessages.push_back({ juce::MidiMessage::channelPressureChange(channel, 0), 0 });
                        localMessages.push_back({ juce::MidiMessage::pitchWheel(channel, 8192), 0 });
                    }
                }
            }

            keyState.status = KeyStatus::Off;
            keyState.messageCount = 0;
            keyState.isLatchOn = false;
            keyState.ehPressureHistory.clear();
            keyState.ehRoll = 0.0f;
            keyState.ehYaw = 0.0f;
        }
    }

    {
        const juce::ScopedLock pendingLock(pendingMessageLock_);
        pendingMidiMessages_.insert(pendingMidiMessages_.end(), localMessages.begin(), localMessages.end());
    }
}

void MidiService::createNoteHold(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime) {
    int channel = state->midiChannel;
    if (channel > 0 && channel <= 16 && (chanNotePri_[channel - 1].empty() || chanNotePri_[channel - 1].front() == keyLookup.keyId)) {
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehRoll, keyLookup.roll, keyLookup.pbRange, state->activeNotes[0], buffer, true, ExpressionCurveTarget::Roll, eventTime);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehYaw, keyLookup.yaw, keyLookup.pbRange, state->activeNotes[0], buffer, true, ExpressionCurveTarget::Yaw, eventTime);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehPressureHistory.back(), keyLookup.pressure, keyLookup.pbRange, state->activeNotes[0], buffer, false, ExpressionCurveTarget::Pressure, eventTime);
    }
    state->messageCount = 0;
}

void MidiService::addMidiValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer& buffer, bool isBipolar, ExpressionCurveTarget curveTarget, int eventTime) {
    if (midiValue.valueType == MidiValueType::Off || channel < 1 || channel > 16) return;
    
    float gain = 1.7f;
    if (!isBipolar && midiValue.valueType == MidiValueType::CC) gain = 1.0f;
    float normalized = isBipolar ? (std::clamp(ehValue * 1.7f, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    normalized = applyExpressionCurve(deviceType, curveTarget, normalized, isBipolar);
    
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
        buffer.addEvent(msg, eventTime);
}

void MidiService::recordVisualEvent(InstrumentType deviceType, ExpressionCurveTarget target, float value, int keyId) {
    int deviceIndex = static_cast<int>(deviceType) - 1;
    int targetIndex = static_cast<int>(target);
    if (deviceIndex < 0 || deviceIndex > 2 || targetIndex < 0 || targetIndex > 5) return;
    
    const juce::ScopedLock sl(visualEventsLock_);
    auto& events = recentVisualEvents_[deviceIndex][targetIndex];
    
    juce::uint32 now = juce::Time::getMillisecondCounter();
    events.push_back({value, now, keyId});
    
    if (events.size() > 20) {
        events.erase(events.begin());
    }
}

std::vector<MidiService::VisualMarker> MidiService::getVisualMarkers(InstrumentType deviceType, ExpressionCurveTarget target) const {
    std::vector<VisualMarker> markers;
    int deviceIndex = static_cast<int>(deviceType) - 1;
    if (deviceIndex < 0 || deviceIndex > 2) return markers;

    juce::uint32 now = juce::Time::getMillisecondCounter();

    if (target == ExpressionCurveTarget::Pressure || target == ExpressionCurveTarget::Yaw || target == ExpressionCurveTarget::Roll) {
        const juce::ScopedLock stateGuard(stateLock_);
        for (int c = 0; c < 3; ++c) {
            for (int k = 0; k < 120; ++k) {
                const auto& state = keyStates_[deviceIndex][c][k];
                if (state.status == KeyStatus::Active) {
                    float val = 0.0f;
                    if (target == ExpressionCurveTarget::Pressure) {
                        if (!state.ehPressureHistory.empty()) val = state.ehPressureHistory.back();
                    } else if (target == ExpressionCurveTarget::Yaw) {
                        val = state.ehYaw;
                    } else if (target == ExpressionCurveTarget::Roll) {
                        val = state.ehRoll;
                    }
                    
                    float gain = 1.7f;
                    bool isBipolar = (target == ExpressionCurveTarget::Yaw || target == ExpressionCurveTarget::Roll);
                    float normalized = isBipolar ? (std::clamp(val * 1.7f, -1.0f, 1.0f)) : (std::clamp(val * gain, 0.0f, 1.0f));
                    
                    markers.push_back({normalized, now, c * 1000 + k});
                }
            }
        }
    } else if (target == ExpressionCurveTarget::Breath) {
        float val = (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex]) 
                           ? 0.0f : ehBreath_[deviceIndex] - breathZeroThreshold_[deviceIndex];
        if (val > 0.0f) {
            val = val / (1.0f - breathZeroThreshold_[deviceIndex]);
        }
        markers.push_back({val, now, -1});
    } else {
        const juce::ScopedLock sl(visualEventsLock_);
        for (const auto& m : recentVisualEvents_[deviceIndex][static_cast<int>(target)]) {
            if ((now - m.timestamp) <= 1500) {
                markers.push_back(m);
            }
        }
    }
    
    return markers;
}

void MidiService::addStripValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer& buffer, bool isBipolar, int eventTime) {
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
        buffer.addEvent(msg, eventTime);
}

void MidiService::createLayoutRPNs(juce::MidiBuffer& buffer) {
    buffer.clear();
    auto buff = juce::MPEMessages::setZoneLayout(mpeZone_);
    buffer.addEvents(buff, 0, -1, 0);
}

float MidiService::calculatePitchBendCurve(float value) const {
    return std::clamp(std::tan(value) / 3.14159265f * 2.0f, -1.0f, 1.0f);
}

float MidiService::applyExpressionCurve(InstrumentType deviceType, ExpressionCurveTarget target, float value, bool isBipolar) const {
    if (deviceType == InstrumentType::None)
        return value;

    int configIndex = static_cast<int>(deviceType) - 1;
    if (configIndex < 0 || configIndex > 2)
        return value;

    const auto& curve = configLookups_[configIndex].expressionCurves[static_cast<int>(target)];
    if (isBipolar) {
        float unipolar = (value + 1.0f) * 0.5f;
        unipolar = curve.getValue(unipolar);
        return unipolar * 2.0f - 1.0f;
    }

    return curve.getValue(value);
}

juce::MPEValue MidiService::calculateNoteOnVelocity(InstrumentType deviceType, KeyState* state) {
    if (state->ehPressureHistory.size() < PRESSURE_HISTORY_LENGTH) return juce::MPEValue::from7BitInt(1);
    
    auto it = state->ehPressureHistory.begin();
    float v1 = (it[1] + it[2]) / 2.0f;
    float v2 = (it[4] + it[5]) / 2.0f;
    float diff = v2 - v1;
    diff = std::clamp(diff, 0.0f, 1.0f);
    int tableIndex = std::clamp(static_cast<int>(diff * 4096.0f), 0, BezierCurve::TABLE_LENGTH - 1);
    float baseVelocity = velocityCurve_.getTableValue(tableIndex);
    recordVisualEvent(deviceType, ExpressionCurveTarget::Velocity, baseVelocity);
    baseVelocity = applyExpressionCurve(deviceType, ExpressionCurveTarget::Velocity, baseVelocity, false);
    return juce::MPEValue::from7BitInt(static_cast<int>(baseVelocity * 126 + 1));
}

juce::MPEValue MidiService::calculateNoteOffVelocity(InstrumentType deviceType, KeyState* state) {
    if (state->ehPressureHistory.empty()) return juce::MPEValue::from7BitInt(0);
    float norm = std::min(state->ehPressureHistory.front() * 10.0f, 1.0f);
    recordVisualEvent(deviceType, ExpressionCurveTarget::ReleaseVelocity, norm);
    norm = applyExpressionCurve(deviceType, ExpressionCurveTarget::ReleaseVelocity, norm, false);
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
