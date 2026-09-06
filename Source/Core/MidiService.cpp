#include "MidiService.h"
#include "HardwareService.h"
#include "Midi1Protocol.h"
#include "Midi2Protocol.h"
#include <cmath>
#include <algorithm>

namespace ecm {

MidiService::MidiService(ConfigLookup (&configLookups)[3], juce::CriticalSection& stateLock)
    : configLookups_(configLookups), stateLock_(stateLock) {
}

MidiService::~MidiService() {
    if (virtualUmpInput_.isAlive())
        virtualUmpInput_.removeConsumer(*this);

    stop();
    
    auto* snap = activeSnapshot_.exchange(nullptr);
    delete snap;

    const juce::ScopedLock sl(deletionQueueLock_);
    deletionQueue_.clear();
}

void MidiService::start(juce::AudioProcessorValueTreeState& pluginState, HardwareService* hs) {
    const juce::ScopedLock stateGuard(stateLock_);
    hardwareService_ = hs;
    pluginState_ = &pluginState;
    int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount(pluginState.state);
    mpeZone_.setLowerZone(lowerChannelCount, 2, SettingsWrapper::getLowerMPEPB(pluginState.state));
    
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(pluginState.state);
        mpeZone_.setUpperZone(upperChannelCount, 2, SettingsWrapper::getUpperMPEPB(pluginState.state));
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
    
    pluginState.state.addListener(this);
    SettingsWrapper::addListener(this, pluginState.state);
    juce::universal_midi_packets::Endpoints::getInstance()->addListener(*this);

    bool midi2 = SettingsWrapper::getMidi2Mode(pluginState.state);
    {
        const juce::ScopedLock sl(umpOutputLock_);
        isMidi2Mode_ = midi2;
    }
    juce::Logger::writeToLog("MidiService: Initializing MIDI Protocol. MIDI 2.0 Mode: " + juce::String(midi2 ? "Enabled" : "Disabled"));

    if (midi2)
        protocol_ = std::make_shared<Midi2Protocol>();
    else
        protocol_ = std::make_shared<Midi1Protocol>();
    
    updateVirtualOutput();
    sendIdentification();
    
    initialized_ = true;
}

void MidiService::stop() {
    juce::universal_midi_packets::Endpoints::getInstance()->removeListener(*this);
    if (pluginState_ != nullptr) {
        pluginState_->state.removeListener(this);
        SettingsWrapper::getSettingsTree(pluginState_->state).removeListener(this);
    }

    initialized_ = false;
    pluginState_ = nullptr;
    {
        const juce::ScopedLock sl(pendingMessageLock_);
        pendingMidiBuffer_.clear();
    }
}

void MidiService::sendIdentification()
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (!protocol_) return;
    
    juce::Logger::writeToLog("MidiService: Sending MIDI protocol identification messages.");
    
    juce::MidiBuffer identBuffer;
    protocol_->addIdentification(identBuffer, 0);
    
    if (identBuffer.isEmpty()) return;

    // Send to the primary selected output
    drainDirectUMPs(identBuffer);
    
    // Also explicitly send to the virtual output if it's alive and NOT already the primary
    if (isFirstInstance_ && directUmpOutput_.isAlive() && !isVirtualTarget_)
    {
        juce::Logger::writeToLog("MidiService: Explicitly sending identification to virtual UMP output.");
        for (const auto meta : identBuffer)
        {
            if (isMidi2Mode_)
            {
                const auto* data = reinterpret_cast<const uint32_t*>(meta.data);
                const size_t numWords = (size_t)meta.numBytes / 4;
                if (numWords > 0)
                {
                    juce::universal_midi_packets::Iterator begin(data, numWords);
                    juce::universal_midi_packets::Iterator end(data + numWords, 0);
                    if (directUmpOutput_.isAlive())
                        directUmpOutput_.send(begin, end);
                }
            }
        }
    }
}

void MidiService::setRuntimeConfigSnapshot(std::unique_ptr<RuntimeConfigSnapshot> snapshot)
{
    RuntimeConfigSnapshot* newPtr = snapshot.release();
    RuntimeConfigSnapshot* oldPtr = activeSnapshot_.exchange(newPtr, std::memory_order_acq_rel);

    if (oldPtr != nullptr)
    {
        const juce::ScopedLock sl(deletionQueueLock_);
        deletionQueue_.push_back({ std::unique_ptr<RuntimeConfigSnapshot>(oldPtr), currentBlockId_.load(std::memory_order_acquire) });
    }

    // Prune deletion queue
    {
        const juce::ScopedLock sl(deletionQueueLock_);
        const uint64_t completedId = currentBlockId_.load(std::memory_order_acquire);
        deletionQueue_.erase(std::remove_if(deletionQueue_.begin(), deletionQueue_.end(),
            [completedId](const DeferredSnapshot& d) {
                return d.blockId < completedId;
            }), deletionQueue_.end());
    }
}

void MidiService::finishedBlock()
{
    currentBlockId_.fetch_add(1, std::memory_order_release);
}

void MidiService::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == SettingsWrapper::id_lowerMPEVoiceCount || 
        property == SettingsWrapper::id_upperMPEVoiceCount ||
        property == SettingsWrapper::id_lowerMPEPB ||
        property == SettingsWrapper::id_upperMPEPB)
    {
        const juce::ScopedLock stateGuard(stateLock_);
        int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount(tree);
        mpeZone_.setLowerZone(lowerChannelCount, 2, SettingsWrapper::getLowerMPEPB(tree));
        
        if (lowerChannelCount < 14) {
            int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(tree);
            mpeZone_.setUpperZone(upperChannelCount, 2, SettingsWrapper::getUpperMPEPB(tree));
        }
    }
}

void MidiService::valueTreeRedirected(juce::ValueTree& tree)
{
    juce::Logger::writeToLog("MidiService: ValueTree redirected. Re-registering listener.");
    tree.addListener(this);
    SettingsWrapper::addListener(this, tree);
    
    // Refresh MPE settings
    const juce::ScopedLock stateGuard(stateLock_);
    int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount(tree);
    mpeZone_.setLowerZone(lowerChannelCount, 2, SettingsWrapper::getLowerMPEPB(tree));
    
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(tree);
        mpeZone_.setUpperZone(upperChannelCount, 2, SettingsWrapper::getUpperMPEPB(tree));
    }
}

void MidiService::processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, int eventTime, int* presetSlotRequest) {
    if (!initialized_) return;

    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    MidiProtocol* protocol = snapshot->protocol.get();
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
                processNoteKey(oscMsg, keyLookup, keyState, midiBuffer, eventTime, protocol);
            else if (keyLookup.mapType == KeyMappingType::MidiMsg)
                processCmdKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer, eventTime, protocol);
            else if (keyLookup.mapType == KeyMappingType::AppCtrl)
                processAppCtrlKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer, eventTime, presetSlotRequest);
            break;
        }
        case osc::MessageType::Breath: {
            float prevBreathValue = ehBreath_[deviceIndex];
            ehBreath_[deviceIndex] = std::abs(oscMsg.value);
            if ((ehBreath_[deviceIndex] > breathZeroThreshold_[deviceIndex]) || 
                (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex] && prevBreathValue > 0.0f)) {
                createBreath(deviceIndex, deviceLookups, midiBuffer, eventTime, protocol);
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
                    createStripAbsolute(deviceIndex, stripIndex, i, deviceLookups, midiBuffer, eventTime, protocol);
                }
                createStripRelative(deviceIndex, stripIndex, i, deviceLookups, midiBuffer, eventTime, protocol);
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

void MidiService::processNoteKey(const osc::Message& oscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    state->messageCount++;

    if (!oscMsg.active) {
        createNoteOff(keyLookup, state, buffer, eventTime, protocol);
    } else if (state->status == KeyStatus::Off) {
        state->status = KeyStatus::Pending;
    } else if (state->messageCount == PRESSURE_HISTORY_LENGTH && state->status == KeyStatus::Pending) {
        createNoteOn(keyLookup, state, buffer, eventTime, protocol);
    } else if (state->status == KeyStatus::Active) {
        if (state->messageCount >= 64)
            createNoteHold(keyLookup, state, buffer, eventTime, protocol);
    }
}

void MidiService::processCmdKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    if (!oscMsg.active) {
        if (keyLookup.cmdType == 2) // Momentary
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg, oscMsg.devId, eventTime, protocol);
    } else if (state->status == KeyStatus::Off) {
        if (keyLookup.cmdType == 1 && state->isLatchOn)
            createMidiMsgOff(keyLookup, state, buffer, outgoingOscMsg, oscMsg.devId, eventTime, protocol);
        else
            createMidiMsgOn(keyLookup, state, buffer, outgoingOscMsg, oscMsg.devId, eventTime, protocol);
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
    const juce::ScopedLock sl(pendingMessageLock_);
    const juce::ScopedLock sl2(umpOutputLock_);
    
    auto& output = isVirtualTarget_ ? directUmpOutput_ : umpOutput_;

    if (output.isAlive())
    {
        drainDirectUMPs(pendingMidiBuffer_);
    }
    else
    {
        buffer.addEvents(pendingMidiBuffer_, 0, -1, eventTime);
    }
    
    pendingMidiBuffer_.clear();
}

void MidiService::drainDirectUMPs(juce::MidiBuffer& buffer)
{
    const juce::ScopedLock sl(umpOutputLock_);

    auto& output = isVirtualTarget_ ? directUmpOutput_ : umpOutput_;

    if (output.isAlive())
    {
        if (!buffer.isEmpty())
            juce::Logger::writeToLog("MidiService: Draining " + juce::String(buffer.getNumEvents()) + " events to " + (isVirtualTarget_ ? "direct/virtual" : "primary") + " output.");

        for (const auto meta : buffer)
        {
            if (isMidi2Mode_)
            {
                const auto* data = reinterpret_cast<const uint32_t*>(meta.data);
                const size_t numWords = (size_t)meta.numBytes / 4;
                
                if (numWords > 0)
                {
                    juce::universal_midi_packets::Iterator begin(data, numWords);
                    juce::universal_midi_packets::Iterator end(data + numWords, 0);
                    if (output.isAlive())
                        output.send(begin, end);
                }
            }
            else
            {
                auto msg = meta.getMessage();
                auto size = (size_t)msg.getRawDataSize();
                const uint8_t* data = msg.getRawData();

                bool isValidMidi1 = false;
                if (size > 0 && size <= 3 && data[0] >= 0x80)
                    isValidMidi1 = true;
                else if (size >= 2 && data[0] == 0xf0)
                    isValidMidi1 = true;

                // Safety check: toMidi1 expects a valid MIDI 1.0 bytestream message (1-3 bytes or SysEx)
                if (isValidMidi1)
                {
                    juce::universal_midi_packets::Conversion::toMidi1({ umpGroup_, juce::Span<const std::byte>(reinterpret_cast<const std::byte*>(data), size) }, [&](const juce::universal_midi_packets::View& view) {
                        juce::universal_midi_packets::Iterator begin(view.data(), view.size());
                        juce::universal_midi_packets::Iterator end(view.data() + view.size(), 0);
                        if (output.isAlive())
                            output.send(begin, end);
                    });
                }
                else
                {
                    juce::Logger::writeToLog("MidiService: Skipping message of size " + juce::String((int)size) + " Status=0x" + juce::String::toHexString(size > 0 ? (int)data[0] : 0) + " in MIDI 1.0 mode (likely stale UMP data).");
                }
            }
        }
    }
    else if (!buffer.isEmpty())
    {
        juce::Logger::writeToLog("MidiService: Target UMP Output not alive, dropping " + juce::String(buffer.getNumEvents()) + " events.");
    }
}

juce::universal_midi_packets::EndpointId MidiService::getCorrectedEndpointId(juce::universal_midi_packets::EndpointId id, const juce::String& name)
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (name.contains("ECMapper Virtual Out") || name.contains("ECMapper Direct"))
    {
        if (isMidi2Mode_)
        {
            if (virtualEndpoint_.isAlive())
                return virtualEndpoint_.getId();
        }
        else if (virtualUmpOutput_.has_value() && *virtualUmpOutput_)
        {
            return virtualUmpOutput_->getId();
        }
    }
    return id;
}

juce::universal_midi_packets::EndpointId MidiService::getEndpointIdSafe(juce::MidiOutput* output)
{
    if (output == nullptr)
        return {};

    auto identifier = output->getIdentifier();
    if (identifier.isEmpty())
        return {};

    // First try to see if it's one of our known virtual outputs
    if (auto id = getCorrectedEndpointId({}, output->getName()); id != juce::universal_midi_packets::EndpointId{})
        return id;

    // For other outputs, try to find the matching endpoint by identifier in the global list
    for (const auto& id : juce::universal_midi_packets::Endpoints::getInstance()->getEndpoints())
    {
        if (id.src == identifier || id.dst == identifier)
            return id;
    }

    return {};
}

void MidiService::setMidiOutput(juce::MidiOutput* output)
{
    if (output == nullptr)
    {
        const juce::ScopedLock sl(umpOutputLock_);
        umpOutput_ = {};
        lastEndpointId_ = {};
        midiOutputName_ = "None";
        isVirtualTarget_ = false;
        return;
    }

    auto name = output->getName();
    auto virtualTarget = (name.contains("ECMapper Virtual Out") || name.contains("ECMapper Direct"));
    auto endpointId = getEndpointIdSafe(output);
    auto group = output->getGroup();

    const juce::ScopedLock sl(umpOutputLock_);
    midiOutputName_ = name;
    isVirtualTarget_ = virtualTarget;
    lastEndpointId_ = endpointId;
    umpGroup_ = group;
    
    if (isVirtualTarget_)
    {
        juce::Logger::writeToLog("MidiService: Selected output is virtual (" + midiOutputName_ + "). Using internal direct connection.");
        umpOutput_ = {};
        
        // Ensure directUmpOutput_ is alive if we are the first instance
        if (isFirstInstance_ && (!directUmpOutput_.isAlive()))
        {
            if (isMidi2Mode_)
            {
                if (virtualEndpoint_.isAlive())
                {
                    directUmpOutput_ = umpSession_->connectOutput(virtualEndpoint_.getId());
                }
            }
            else if (virtualUmpOutput_.has_value() && *virtualUmpOutput_)
            {
                directUmpOutput_ = umpSession_->connectOutput(virtualUmpOutput_->getId());
            }
        }
        return;
    }

    if (!umpSession_.has_value() || !umpOutput_.isAlive())
    {
        if (!umpSession_.has_value())
            umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");
            
        juce::Logger::writeToLog("MidiService: Attempting to connect UMP output to ID: src='" + endpointId.src + "', dst='" + endpointId.dst + "'");
        
        umpOutput_ = (*umpSession_).connectOutput(endpointId);
        
        if (umpOutput_.isAlive())
            juce::Logger::writeToLog("MidiService: Connected direct UMP output to " + name);
        else
            juce::Logger::writeToLog("MidiService: Failed to connect direct UMP output to " + name + ". Will retry if endpoints change.");
            
        sendIdentification();
    }
}

void MidiService::endpointsChanged()
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (isVirtualTarget_) return;

    if (lastEndpointId_ != juce::universal_midi_packets::EndpointId{} && !umpOutput_.isAlive())
    {
        if (!umpSession_.has_value())
            umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");

        umpOutput_ = (*umpSession_).connectOutput(lastEndpointId_);

        if (umpOutput_.isAlive())
            juce::Logger::writeToLog("MidiService: Automatically connected direct UMP output to " + midiOutputName_ + " after endpoint change.");
    }
}

void MidiService::consume (juce::universal_midi_packets::Iterator b, juce::universal_midi_packets::Iterator e, double time)
{
    juce::ignoreUnused (time);
    bool shouldRespond = false;
    for (auto it = b; it != e; ++it)
    {
        using namespace juce::universal_midi_packets;
        View view (*it);
        if (universal_midi_packets::Utils::getMessageType (view[0]) == universal_midi_packets::Utils::MessageKind::stream)
        {
            auto status = universal_midi_packets::Utils::U8<1>::get (view[0]);
            juce::Logger::writeToLog("MidiService: Received Stream Message 0x" + juce::String::toHexString((int)status) + 
                                     " size=" + juce::String(view.size()));
            
            if (status == 0x00 || status == 0x05 || status == 0x10) // Endpoint Discovery, Stream Config Request, or Function Block Discovery
            {
                shouldRespond = true;
                
                if (status == 0x05 && view.size() > 1)
                {
                    auto requestedProtocol = (view[1] >> 24) & 0xFF;
                    juce::Logger::writeToLog("MidiService: Host requested protocol 0x" + juce::String::toHexString((int)requestedProtocol));
                }
            }
        }
    }

    if (shouldRespond)
    {
        juce::Logger::writeToLog ("MidiService: Responding to MIDI 2.0 Discovery/Config request.");
        sendIdentification();
    }
}

void MidiService::updateVirtualOutput()
{
    juce::Logger::writeToLog("MidiService::updateVirtualOutput() called. Mode: " + juce::String(isMidi2Mode_ ? "MIDI 2.0" : "MIDI 1.0"));
#if JUCE_MAC
    if (!juce::JUCEApplicationBase::isStandaloneApp())
    {
        juce::Logger::writeToLog("MidiService: Not a standalone app, skipping virtual MIDI port creation.");
        return;
    }

    if (virtualMidiLock_ == nullptr)
    {
        juce::Logger::writeToLog("MidiService: Initializing InterProcessLock for virtual MIDI.");
        virtualMidiLock_ = std::make_unique<juce::InterProcessLock>("ECMapper_VirtualMidi_Lock");
    }

    isFirstInstance_ = virtualMidiLock_->enter(0);

    if (isFirstInstance_)
    {
        juce::Logger::writeToLog("MidiService: This is the first instance. Managing virtual port.");

        const juce::ScopedLock sl(umpOutputLock_);

        // Clean up ports that don't match the current mode
        if (isMidi2Mode_)
        {
            if (virtualUmpOutput_.has_value() && *virtualUmpOutput_)
            {
                juce::Logger::writeToLog("MidiService: Closing legacy virtual output.");
                virtualUmpOutput_ = {};
                virtualUmpInputMirror_ = {};
            }
        }
        else
        {
            if (virtualEndpoint_.isAlive())
            {
                juce::Logger::writeToLog("MidiService: Closing UMP virtual endpoint.");
                if (virtualUmpInput_.isAlive())
                    virtualUmpInput_.removeConsumer(*this);
                virtualEndpoint_ = {};
                virtualUmpInput_ = {};
            }
        }
        
        bool needsCreation = false;
        if (isMidi2Mode_)
            needsCreation = !virtualEndpoint_.isAlive();
        else
            needsCreation = !virtualUmpOutput_.has_value() || !*virtualUmpOutput_;

        if (needsCreation)
        {
            if (!umpSession_.has_value())
            {
                juce::Logger::writeToLog("MidiService: Creating UMP session.");
                umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");
            }

            if (umpSession_.has_value())
            {
                if (isMidi2Mode_)
                {
                    using namespace juce::universal_midi_packets;
                    juce::Logger::writeToLog("MidiService: Creating VirtualEndpoint (MIDI 2.0) 'ECMapper Direct'.");
                    
                    DeviceInfo devInfo;
                    devInfo.manufacturer = {std::byte{0x7D}, std::byte{0x00}, std::byte{0x00}};
                    devInfo.family = {std::byte{0x01}, std::byte{0x00}};
                    devInfo.modelNumber = {std::byte{0x01}, std::byte{0x00}};
                    devInfo.revision = {std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

                    const std::array blocks { 
                        Block{}.withName("Main")
                               .withDirection(BlockDirection::bidirectional)
                               .withUiHint(BlockUiHint::bidirectional)
                               .withEnabled(true)
                               .withFirstGroup(0)
                               .withNumGroups(16)
                               .withMIDI1ProxyKind(BlockMIDI1ProxyKind::inapplicable)
                    };
                    
                    virtualEndpoint_ = umpSession_->createVirtualEndpoint("ECMapper Direct UMP", devInfo, "ECMapper-Direct-UMP", PacketProtocol::MIDI_2_0, blocks, BlocksAreStatic::yes);
                    
                    if (virtualEndpoint_.isAlive())
                    {
                        juce::Logger::writeToLog("MidiService: Successfully created virtual MIDI 2.0 endpoint.");
                        directUmpOutput_ = umpSession_->connectOutput(virtualEndpoint_.getId());
                        
                        virtualUmpInput_ = umpSession_->connectInput(virtualEndpoint_.getId(), PacketProtocol::MIDI_2_0);
                        virtualUmpInput_.addConsumer(*this);
                        
                        sendIdentification();
                        endpointsChanged();
                    }
                    else
                    {
                        juce::Logger::writeToLog("MidiService: Failed to create virtual MIDI 2.0 endpoint.");
                    }
                }
                else
                {
                    juce::Logger::writeToLog("MidiService: Creating LegacyVirtualOutput 'ECMapper Direct'.");
                    virtualUmpOutput_ = umpSession_->createLegacyVirtualOutput("ECMapper Direct");
                    
                    juce::Logger::writeToLog("MidiService: Creating LegacyVirtualInput mirror 'ECMapper Virtual Out'.");
                    virtualUmpInputMirror_ = umpSession_->createLegacyVirtualInput("ECMapper Virtual Out");

                    if (virtualUmpOutput_.has_value() && *virtualUmpOutput_)
                    {
                        juce::Logger::writeToLog("MidiService: Successfully created virtual MIDI output 'ECMapper Direct'");
                        directUmpOutput_ = umpSession_->connectOutput(virtualUmpOutput_->getId());
                        
                        sendIdentification();
                        endpointsChanged();
                    }
                }
            }
            else
            {
                juce::Logger::writeToLog("MidiService: Failed to create UMP session.");
            }
        }
        else
        {
            juce::Logger::writeToLog("MidiService: Virtual port already exists.");
        }
    }
    else
    {
        juce::Logger::writeToLog("MidiService: Not the first instance (lock busy), not creating virtual MIDI output.");
    }
#else
    juce::Logger::writeToLog("MidiService: Virtual MIDI output only supported on macOS.");
#endif
}

bool MidiService::isVirtualOutputActive() const
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (isMidi2Mode_)
        return virtualEndpoint_.isAlive();
    return virtualUmpOutput_.has_value() && *virtualUmpOutput_;
}

bool MidiService::isUsingUMPPath() const
{
    const juce::ScopedLock sl(umpOutputLock_);
    return umpOutput_.isAlive();
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
    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    MidiProtocol* protocol = snapshot->protocol.get();
    const auto& runtimeLookups = snapshot->configLookups;
    for (int i = 0; i < 3; i++) {
        if (ehBreath_[i] <= 0.0f) continue;
        ehBreath_[i] = (ehBreath_[i] > breathZeroThreshold_[i]) ? ehBreath_[i] - 0.005f : 0.0f;
        createBreath(i, runtimeLookups[i], buffer, eventTime, protocol);
    }
}

void MidiService::createBreath(int deviceIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    float val = (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex]) 
                       ? 0.0f : ehBreath_[deviceIndex] - breathZeroThreshold_[deviceIndex];
    
    // Scale val to [0, 1] range after threshold
    if (val > 0.0f) {
        val = val / (1.0f - breathZeroThreshold_[deviceIndex]);
    }

    for (int z = 0; z < 3; ++z) {
        addMidiValueMessage(static_cast<InstrumentType>(deviceIndex + 1), keyLookup.breath[z].channel, val * 3.0f, keyLookup.breath[z].midiValue, 1.0f, 0, buffer, false, ExpressionCurveTarget::Breath, eventTime, protocol);
    }
}

void MidiService::createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    addStripValueMessage(strip.channel, ehStrips_[stripIndex][deviceIndex], strip.absMidiValue, buffer, false, eventTime, protocol);
}

void MidiService::createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    float relValue = (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) 
                   ? 0.0f : relStart_ehStrips_[stripIndex][deviceIndex] - ehStrips_[stripIndex][deviceIndex];
    
    if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
        currentStripPBperChannel_[strip.channel > 0 ? strip.channel - 1 : 0] = 0;
    }

    addStripValueMessage(strip.channel, relValue, strip.relMidiValue, buffer, true, eventTime, protocol);
}

void MidiService::createNoteOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    int deviceIndex = static_cast<int>(keyLookup.keyId.deviceType) - 1;
    int totalTranspose = (deviceIndex >= 0 && deviceIndex < 3) ? (latchTranspose_[deviceIndex] + momentaryTranspose_[deviceIndex]) : 0;

    state->midiChannel = protocol ? protocol->findMidiChannelForNewNote(keyLookup.output, keyLookup.notes[0]) : static_cast<int>(keyLookup.output);

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

    createNoteHold(keyLookup, state, buffer, eventTime, protocol);
    float vel = calculateNoteOnVelocity(keyLookup.keyId.deviceType, state);
    
    for (int i = 0; i < 4; i++) {
        int noteNo = state->activeNotes[i];
        if (noteNo > -1) {
            if (countPlayingNoteMatches(state->midiChannel, noteNo) == 0) {
                if (protocol) protocol->addNoteOn(buffer, state->midiChannel, noteNo, vel, eventTime);
            }
            playingNotes_.push_back({state->midiChannel, noteNo});
        }
    }
    state->status = KeyStatus::Active;
}

void MidiService::createNoteOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    int channel = state->midiChannel;
    if (protocol) protocol->releaseMidiChannel(keyLookup.output, keyLookup.notes[0], channel);

    if (channel > 0 && channel <= 16) {
        chanNotePri_[channel - 1].remove_if([&keyLookup](const LayoutWrapper::KeyId& id) { return id == keyLookup.keyId; });
    }

    float vel = calculateNoteOffVelocity(keyLookup.keyId.deviceType, state);
    for (int i = 0; i < 4; i++) {
        int noteToTurnOff = state->activeNotes[i];
        if (noteToTurnOff > -1) {
            if (countPlayingNoteMatches(channel, noteToTurnOff) < 2) {
                if (protocol) protocol->addNoteOff(buffer, channel, noteToTurnOff, vel, eventTime);
            }
            removeOneNoteMatch(channel, noteToTurnOff);
            state->activeNotes[i] = -1;
        }
    }
    
    if (channel > 0 && channel <= 16 && chanNotePri_[channel - 1].empty()) {
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], buffer, false, ExpressionCurveTarget::Pressure, eventTime, protocol);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], buffer, true, ExpressionCurveTarget::Roll, eventTime, protocol);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], buffer, true, ExpressionCurveTarget::Yaw, eventTime, protocol);
    }
    state->status = KeyStatus::Off;
    state->messageCount = 0;
}

void MidiService::createMidiMsgOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiProtocol* protocol) {
    state->isLatchOn = true;
    state->midiChannel = protocol ? protocol->findMidiChannelForNewNote(keyLookup.output, -1) : 
                         ((keyLookup.output == MidiChannelType::MPE_Low) ? 1 : 
                          (keyLookup.output == MidiChannelType::MPE_High) ? 16 : static_cast<int>(keyLookup.output));

    if (keyLookup.msgType == 4) {
        createAllNotesOff(buffer, eventTime, protocol);
    } else if (keyLookup.msgType == 1) {
        if (protocol) protocol->addCC(buffer, state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOn / 127.0f, eventTime);
    } else if (keyLookup.msgType == 2) {
        if (protocol) protocol->addProgramChange(buffer, state->midiChannel, keyLookup.cmdOn, eventTime);
    } else if (keyLookup.msgType == 3) {
        if (protocol) {
            if (keyLookup.cmdOn == 1) protocol->addMidiStart(buffer, eventTime);
            else if (keyLookup.cmdOn == 2) protocol->addMidiStop(buffer, eventTime);
            else if (keyLookup.cmdOn == 3) protocol->addMidiContinue(buffer, eventTime);
        }
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

void MidiService::createMidiMsgOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiProtocol* protocol) {
    if (keyLookup.cmdType != 3) { // Not Trigger
        if (keyLookup.msgType == 4) {
            createAllNotesOff(buffer, eventTime, protocol);
        } else if (keyLookup.msgType == 1) {
            if (protocol) protocol->addCC(buffer, state->midiChannel, keyLookup.cmdCC, keyLookup.cmdOff / 127.0f, eventTime);
        } else if (keyLookup.msgType == 2) {
            if (protocol) protocol->addProgramChange(buffer, state->midiChannel, keyLookup.cmdOff, eventTime);
        } else if (keyLookup.msgType == 3) {
            if (protocol) {
                if (keyLookup.cmdOff == 1) protocol->addMidiStart(buffer, eventTime);
                else if (keyLookup.cmdOff == 2) protocol->addMidiStop(buffer, eventTime);
                else if (keyLookup.cmdOff == 3) protocol->addMidiContinue(buffer, eventTime);
            }
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

void MidiService::createAllNotesOff(juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    for (int i = 1; i <= 16; i++) {
        if (protocol) protocol->addAllNotesOff(buffer, i, eventTime);
        chanNotePri_[i - 1].clear();
    }
    playingNotes_.clear();
}

void MidiService::appendPendingMidiMessage(const juce::MidiMessage& message, int eventTime) {
    const juce::ScopedLock sl(pendingMessageLock_);
    pendingMidiBuffer_.addEvent(message, eventTime);
}

void MidiService::queueTransposeChangeFlush(InstrumentType deviceType, Zone zone) {
    if (!initialized_ || pluginState_ == nullptr || deviceType == InstrumentType::None || zone == Zone::NoZone)
        return;

    const juce::ScopedLock stateGuard(stateLock_);

    int deviceIndex = static_cast<int>(deviceType) - 1;
    jassert(deviceIndex >= 0 && deviceIndex < 3);
    if (deviceIndex < 0 || deviceIndex > 2)
        return;

    juce::MidiBuffer localMessages;
    MidiProtocol* protocol = protocol_.get();

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
                        if (countPlayingNoteMatches(channel, keyLookup.notes[i]) < 2) {
                            if (protocol) protocol->addNoteOff(localMessages, channel, keyLookup.notes[i], vel, 0);
                        }
                        removeOneNoteMatch(channel, keyLookup.notes[i]);
                    }
                }

                if (protocol) protocol->releaseMidiChannel(keyLookup.output, keyLookup.notes[0], channel);

                if (channel > 0 && channel <= 16) {
                    chanNotePri_[channel - 1].remove_if([&keyId](const LayoutWrapper::KeyId& id) { return id == keyId; });

                    if (chanNotePri_[channel - 1].empty()) {
                        currentKeyPBperChannel_[channel - 1] = 0.0f;
                        currentStripPBperChannel_[channel - 1] = 0.0f;
                        if (protocol) {
                            protocol->addChannelPressure(localMessages, channel, 0.0f, 0);
                            protocol->addPitchBend(localMessages, channel, -1, 0.5f, 0);
                        }
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
        pendingMidiBuffer_.addEvents(localMessages, 0, -1, 0);
    }
}

void MidiService::createNoteHold(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol) {
    int channel = state->midiChannel;
    if (channel > 0 && channel <= 16 && (chanNotePri_[channel - 1].empty() || chanNotePri_[channel - 1].front() == keyLookup.keyId)) {
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehRoll, keyLookup.roll, keyLookup.pbRange, state->activeNotes[0], buffer, true, ExpressionCurveTarget::Roll, eventTime, protocol);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehYaw, keyLookup.yaw, keyLookup.pbRange, state->activeNotes[0], buffer, true, ExpressionCurveTarget::Yaw, eventTime, protocol);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehPressureHistory.back(), keyLookup.pressure, keyLookup.pbRange, state->activeNotes[0], buffer, false, ExpressionCurveTarget::Pressure, eventTime, protocol);
    }
    state->messageCount = 0;
}

void MidiService::addMidiValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer& buffer, bool isBipolar, ExpressionCurveTarget curveTarget, int eventTime, MidiProtocol* protocol) {
    if (midiValue.valueType == MidiValueType::Off || channel < 1 || channel > 16) return;
    
    float gain = 1.7f;
    if (!isBipolar && midiValue.valueType == MidiValueType::CC) gain = 1.0f;
    float normalized = isBipolar ? (std::clamp(ehValue * 1.7f, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    normalized = applyExpressionCurve(deviceType, curveTarget, normalized, isBipolar);
    
    if (!protocol) return;

    if (midiValue.valueType == MidiValueType::Pitchbend) {
        currentKeyPBperChannel_[channel - 1] = calculatePitchBendCurve(normalized) * pbRange;
        float totalPB = std::clamp(currentKeyPBperChannel_[channel - 1] + currentStripPBperChannel_[channel - 1], -1.0f, 1.0f);
        protocol->addPitchBend(buffer, channel, noteNo, totalPB * 0.5f + 0.5f, eventTime);
    } else if (midiValue.valueType == MidiValueType::ChannelAftertouch) {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        protocol->addChannelPressure(buffer, channel, val, eventTime);
    } else if (midiValue.valueType == MidiValueType::PolyAftertouch) {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        protocol->addPolyAftertouch(buffer, channel, noteNo, val, eventTime);
    } else if (midiValue.valueType == MidiValueType::CC) {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        protocol->addCC(buffer, channel, midiValue.ccNo, val, eventTime);
    }
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

void MidiService::addStripValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer& buffer, bool isBipolar, int eventTime, MidiProtocol* protocol) {
    if (midiValue.valueType == MidiValueType::Off || channel < 1 || channel > 16) return;
    
    float gain = isBipolar ? 1.7f : 1.0f;
    float normalized = isBipolar ? (std::clamp(ehValue * gain, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    
    if (!protocol) return;

    if (midiValue.valueType == MidiValueType::Pitchbend) {
        currentStripPBperChannel_[channel - 1] = (isBipolar ? calculatePitchBendCurve(normalized) : normalized);
        float totalPB = std::clamp(currentKeyPBperChannel_[channel - 1] + currentStripPBperChannel_[channel - 1], -1.0f, 1.0f);
        protocol->addPitchBend(buffer, channel, -1, totalPB * 0.5f + 0.5f, eventTime);
    } else {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        if (midiValue.valueType == MidiValueType::ChannelAftertouch)
            protocol->addChannelPressure(buffer, channel, val, eventTime);
        else if (midiValue.valueType == MidiValueType::CC)
            protocol->addCC(buffer, channel, midiValue.ccNo, val, eventTime);
    }
}

void MidiService::createLayoutRPNs(juce::MidiBuffer& buffer) {
    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    MidiProtocol* protocol = snapshot ? snapshot->protocol.get() : protocol_.get();
    buffer.clear();
    if (protocol)
        protocol->setup(buffer, mpeZone_);
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

float MidiService::calculateNoteOnVelocity(InstrumentType deviceType, KeyState* state) {
    if (state->ehPressureHistory.size() < PRESSURE_HISTORY_LENGTH) return 0.0f;
    
    auto it = state->ehPressureHistory.begin();
    float v1 = (it[1] + it[2]) / 2.0f;
    float v2 = (it[4] + it[5]) / 2.0f;
    float diff = v2 - v1;
    diff = std::clamp(diff, 0.0f, 1.0f);
    int tableIndex = std::clamp(static_cast<int>(diff * 4096.0f), 0, BezierCurve::TABLE_LENGTH - 1);
    float baseVelocity = velocityCurve_.getTableValue(tableIndex);
    recordVisualEvent(deviceType, ExpressionCurveTarget::Velocity, baseVelocity);
    baseVelocity = applyExpressionCurve(deviceType, ExpressionCurveTarget::Velocity, baseVelocity, false);
    
    // Minimum MIDI 1.0 velocity is 1. We should probably keep that floor even in high resolution
    // to avoid accidental note-offs if something maps 0 to NoteOn.
    return std::max(baseVelocity, 1.0f / 127.0f);
}

float MidiService::calculateNoteOffVelocity(InstrumentType deviceType, KeyState* state) {
    if (state->ehPressureHistory.empty()) return 0.0f;
    float norm = std::min(state->ehPressureHistory.front() * 10.0f, 1.0f);
    recordVisualEvent(deviceType, ExpressionCurveTarget::ReleaseVelocity, norm);
    norm = applyExpressionCurve(deviceType, ExpressionCurveTarget::ReleaseVelocity, norm, false);
    return norm;
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
