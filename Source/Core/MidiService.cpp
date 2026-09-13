#include "MidiService.h"
#include "ExpressionPreprocessor.h"
#include "HardwareService.h"
#include "Midi1Protocol.h"
#include "Midi2Protocol.h"
#include "VoiceAllocator.h"
#include <cmath>
#include <algorithm>
#include <set>

namespace ecm {

namespace {

constexpr int defaultMPEMasterPitchbendRange = 12;

}

MidiService::MidiService(ConfigLookup (&configLookups)[3], juce::CriticalSection& stateLock)
    : configLookups_(configLookups), stateLock_(stateLock) {
}

MidiService::~MidiService() {
    for (auto& virtualUmpInput : virtualUmpInputs_)
        if (virtualUmpInput.isAlive())
            virtualUmpInput.removeConsumer(*this);

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
    mpeZone_.setLowerZone(lowerChannelCount, SettingsWrapper::getLowerMPEPB(pluginState.state), defaultMPEMasterPitchbendRange);
    
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(pluginState.state);
        mpeZone_.setUpperZone(upperChannelCount, SettingsWrapper::getUpperMPEPB(pluginState.state), defaultMPEMasterPitchbendRange);
    }
    
    for (int i = 0; i < 3; ++i) {
        configLookups_[i].updateAll();
        latchTranspose_[i] = 0;
        momentaryTranspose_[i] = 0;
        for (int c = 0; c < 3; ++c) {
            for (int k = 0; k < 120; ++k) {
                keyStates_[i][c][k].isLatchOn = false;
                keyStates_[i][c][k].status = KeyStatus::Off;
                keyStates_[i][c][k].messageCount = 0;
                keyStates_[i][c][k].ehPressureHistory.clear();
                keyStates_[i][c][k].ehRoll = 0.0f;
                keyStates_[i][c][k].ehYaw = 0.0f;
                keyStates_[i][c][k].lastTimestamp = 0;
                keyStates_[i][c][k].noteOnTimestamp = 0;
                for (int n = 0; n < 4; ++n)
                    keyStates_[i][c][k].activeNotes[n] = -1;
            }
        }
    }

    ehBreath_[0] = ehBreath_[1] = ehBreath_[2] = 0.0f;
    for (int i = 0; i < 3; ++i)
        breathSamplesSinceUpdate_[i] = BREATH_STABILITY_HOLD_SAMPLES;
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

    transportSession_ = std::dynamic_pointer_cast<MidiTransportSession>(protocol_);
    voiceRouter_ = std::make_shared<ChannelVoiceRouter>(midi2 ? OutputTransportMode::UmpMidi : OutputTransportMode::LegacyMidi);
    expressionPolicy_ = createExpressionEmissionPolicy(midi2 ? OutputTransportMode::UmpMidi : OutputTransportMode::LegacyMidi);
    
    {
        const juce::ScopedLock sl(pendingMessageLock_);
        if (transportSession_)
            transportSession_->setupTransport(pendingMidiBuffer_, mpeZone_);
    }
    if (voiceRouter_)
        voiceRouter_->configureLayout(mpeZone_);

    updateCalibration();

    if (midi2) {
        using namespace juce::midi_ci;
        juce::universal_midi_packets::DeviceInfo info;
        info.manufacturer = {std::byte{0x7D}, std::byte{0x00}, std::byte{0x00}};
        info.family = {std::byte{0x01}, std::byte{0x00}};
        info.modelNumber = {std::byte{0x01}, std::byte{0x00}};
        info.revision = {std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
        
        DeviceOptions options;
        options = options.withDeviceInfo (info)
                         .withOutputs ({ this })
                         .withProfileDelegate (this)
                         .withPropertyDelegate (this)
                         .withFeatures (DeviceFeatures { std::byte { 0x02 | 0x04 | 0x08 } }); // Protocol Negotiation (0x02) + Profile (0x04) + Property (0x08)

        juce::MessageManager::callAsync([weakThis = juce::WeakReference<MidiService>(this), opts = std::move(options)]() mutable {
            if (auto* service = weakThis.get()) {
                service->ciDevice_ = std::make_unique<juce::midi_ci::Device>(opts);
                service->ciDevice_->addListener(*service);
                
                // Advertise MPE profile support
                using namespace juce::midi_ci;
                Profile mpeProfile { std::byte { 0x7E }, std::byte { 0x01 }, std::byte { 0x00 },
                                         std::byte { 0x01 }, std::byte { 0x00 } };

                if (auto* host = service->ciDevice_->getProfileHost())
                {
                    host->addProfile (ProfileAtAddress { mpeProfile, ChannelAddress().withChannel (ChannelInGroup::wholeGroup) }, 0);
                    host->addProfile (ProfileAtAddress { mpeProfile, ChannelAddress().withChannel (ChannelInGroup::channel0) }, 16);
                }
                
                service->ciDevice_->sendDiscovery();
            }
        });
    } else {
        juce::MessageManager::callAsync([weakThis = juce::WeakReference<MidiService>(this)]() {
            if (auto* service = weakThis.get()) {
                service->ciDevice_.reset();
            }
        });
    }
    
    remoteSupportsPerNote_ = false;
    if (voiceRouter_)
        voiceRouter_->setRemoteSupportsPerNote(false);
    logMidiExpressionMode();
    
    updateVirtualOutput();
    sendIdentification();
    
    initialized_ = true;
}

void MidiService::stop() {
    juce::universal_midi_packets::Endpoints::getInstance()->removeListener(*this);
    if (pluginState_ != nullptr) {
        pluginState_->state.removeListener(this);
        SettingsWrapper::removeListener(this, pluginState_->state);
    }

    if (ciDevice_) {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            ciDevice_.reset();
        } else {
            auto* raw = ciDevice_.release();
            juce::MessageManager::callAsync([raw]() { delete raw; });
        }
    }

    if (umpInput_.isAlive())
        umpInput_.removeConsumer(*this);

    {
        const juce::ScopedLock sl(umpOutputLock_);

        for (auto& virtualUmpInput : virtualUmpInputs_)
        {
            if (virtualUmpInput.isAlive())
                virtualUmpInput.removeConsumer(*this);

            virtualUmpInput = {};
        }

        for (auto& virtualEndpoint : virtualEndpoints_)
            virtualEndpoint = {};

        for (auto& directUmpOutput : directUmpOutputs_)
            directUmpOutput = {};

        virtualUmpOutput_ = {};
        virtualUmpInputMirror_ = {};
        isVirtualTarget_ = false;
    }

    if (virtualMidiLock_ != nullptr && isFirstInstance_)
    {
        virtualMidiLock_->exit();
        isFirstInstance_ = false;
    }

    initialized_ = false;
    pluginState_ = nullptr;
    voiceRouter_.reset();
    expressionPolicy_.reset();
    {
        const juce::ScopedLock sl(pendingMessageLock_);
        pendingMidiBuffer_.clear();
    }
}

void MidiService::sendIdentification()
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (!transportSession_) return;
    
    juce::Logger::writeToLog("MidiService: Sending MIDI protocol identification messages.");
    
    juce::MidiBuffer identBuffer;
    transportSession_->addIdentification(identBuffer, 0);
    
    if (identBuffer.isEmpty()) return;

    // Send to the primary selected output
    drainDirectUMPs(identBuffer, true); // Silent if failed, because we might send to virtual next
    
    const bool hasDirectMidi2Output = std::any_of(directUmpOutputs_.begin(), directUmpOutputs_.end(), [](const auto& output) {
        return output.isAlive();
    });

    // Also explicitly send to the virtual outputs if they are alive and NOT already the primary
    if (isFirstInstance_ && hasDirectMidi2Output && !isVirtualTarget_)
    {
        juce::Logger::writeToLog("MidiService: Explicitly sending identification to virtual UMP outputs.");
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

                    for (auto& directUmpOutput : directUmpOutputs_)
                        if (directUmpOutput.isAlive())
                            directUmpOutput.send(begin, end);
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

void MidiService::updateCalibration() {
    if (!pluginState_) return;
    
    struct Defs { InstrumentType type; float breath; float stripT; float stripG; };
    Defs defaults[3] = {
        { InstrumentType::Alpha, 0.03125f, 0.0366f, 1.3f },
        { InstrumentType::Tau,   0.03125f, 0.0366f, 1.3f },
        { InstrumentType::Pico,  0.125f,   0.12f,   1.2f }
    };
    
    for (int i = 0; i < 3; i++) {
        int idx = static_cast<int>(defaults[i].type) - 1;
        breathZeroThreshold_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_breathThreshold, defaults[i].breath, pluginState_->state);
        stripZeroThreshold_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_stripThreshold, defaults[i].stripT, pluginState_->state);
        stripSensitivity_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_stripSensitivity, defaults[i].stripG, pluginState_->state);
        invertStripDirection_[idx] = SettingsWrapper::getCalibrationBool(defaults[i].type, SettingsWrapper::id_invertStripDirection, false, pluginState_->state);
        yawSensitivity_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_yawSensitivity, 1.7f, pluginState_->state);
        rollSensitivity_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_rollSensitivity, 1.7f, pluginState_->state);
        pressureSensitivity_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_pressureSensitivity, 1.7f, pluginState_->state);
        breathSensitivity_[idx] = SettingsWrapper::getCalibrationValue(defaults[i].type, SettingsWrapper::id_breathSensitivity, 1.7f, pluginState_->state);
    }
}

void MidiService::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused(tree);

    const bool mpeVoiceCountChanged = property == SettingsWrapper::id_lowerMPEVoiceCount
                                   || property == SettingsWrapper::id_upperMPEVoiceCount;
    const bool mpePitchbendChanged = property == SettingsWrapper::id_lowerMPEPB
                                  || property == SettingsWrapper::id_upperMPEPB;

    if (mpeVoiceCountChanged || mpePitchbendChanged)
    {
        if (pluginState_ == nullptr)
            return;

        const juce::ScopedLock stateGuard(stateLock_);
        auto& rootState = pluginState_->state;
        int lowerChannelCount = SettingsWrapper::getLowerMPEVoiceCount(rootState);
        mpeZone_.setLowerZone(lowerChannelCount, SettingsWrapper::getLowerMPEPB(rootState), defaultMPEMasterPitchbendRange);
        
        if (lowerChannelCount < 14) {
            int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(rootState);
            mpeZone_.setUpperZone(upperChannelCount, SettingsWrapper::getUpperMPEPB(rootState), defaultMPEMasterPitchbendRange);
        }

        if (mpeVoiceCountChanged && transportSession_) {
            const juce::ScopedLock sl(pendingMessageLock_);
            transportSession_->setupTransport(pendingMidiBuffer_, mpeZone_);
        }
        if (mpeVoiceCountChanged && voiceRouter_)
            voiceRouter_->configureLayout(mpeZone_);
    }
    
    if (property == SettingsWrapper::id_breathThreshold ||
        property == SettingsWrapper::id_breathSensitivity ||
        property == SettingsWrapper::id_stripThreshold ||
        property == SettingsWrapper::id_stripSensitivity ||
        property == SettingsWrapper::id_invertStripDirection ||
        property == SettingsWrapper::id_yawSensitivity ||
        property == SettingsWrapper::id_rollSensitivity ||
        property == SettingsWrapper::id_pressureSensitivity ||
        property == SettingsWrapper::id_calibrationRevision)
    {
        updateCalibration();
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
    mpeZone_.setLowerZone(lowerChannelCount, SettingsWrapper::getLowerMPEPB(tree), defaultMPEMasterPitchbendRange);
    
    if (lowerChannelCount < 14) {
        int upperChannelCount = SettingsWrapper::getUpperMPEVoiceCount(tree);
        mpeZone_.setUpperZone(upperChannelCount, SettingsWrapper::getUpperMPEPB(tree), defaultMPEMasterPitchbendRange);
    }

    if (transportSession_) {
        const juce::ScopedLock sl(pendingMessageLock_);
        transportSession_->setupTransport(pendingMidiBuffer_, mpeZone_);
    }
    if (voiceRouter_)
        voiceRouter_->configureLayout(mpeZone_);
}

void MidiService::processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, int eventTime, int* presetSlotRequest) {
    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    MidiBufferPerformanceEventSink sink { midiBuffer, snapshot->protocol.get() };
    processMessage(oscMsg, outgoingOscMsg, midiBuffer, sink, eventTime, presetSlotRequest);
}

void MidiService::processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, PerformanceEventSink& sink, int eventTime, int* presetSlotRequest) {
    if (!initialized_) return;

    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    MidiVoiceRouter* voiceRouter = snapshot->voiceRouter.get();
    ExpressionEmissionPolicy* expressionPolicy = snapshot->expressionPolicy.get();
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
                processNoteKey(oscMsg, keyLookup, keyState, sink, eventTime, voiceRouter, expressionPolicy);
            else if (keyLookup.mapType == KeyMappingType::MidiMsg)
                processCmdKey(oscMsg, outgoingOscMsg, keyLookup, keyState, sink, eventTime, voiceRouter);
            else if (keyLookup.mapType == KeyMappingType::AppCtrl)
                processAppCtrlKey(oscMsg, outgoingOscMsg, keyLookup, keyState, midiBuffer, eventTime, presetSlotRequest);
            break;
        }
        case osc::MessageType::Breath: {
            float prevBreathValue = ehBreath_[deviceIndex];
            ehBreath_[deviceIndex] = std::abs(oscMsg.value);
            breathSamplesSinceUpdate_[deviceIndex] = 0;
            if ((ehBreath_[deviceIndex] > breathZeroThreshold_[deviceIndex]) || 
                (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex] && prevBreathValue > 0.0f)) {
                createBreath(deviceIndex, deviceLookups, sink, eventTime, voiceRouter);
            }
            break;
        }
        case osc::MessageType::Strip: {
            const int stripIndex = static_cast<int>(oscMsg.strip) - 1;
            if (stripIndex < 0 || stripIndex > 1) break;

            const bool stripOff = !oscMsg.active;
            ehStrips_[stripIndex][deviceIndex] = stripOff
                                              ? 0.0f
                                              : applyStripCalibration(oscMsg.value,
                                                                      invertStripDirection_[deviceIndex],
                                                                      stripZeroThreshold_[deviceIndex],
                                                                      stripSensitivity_[deviceIndex]);
            
            if (stripOff) {
                relStart_ehStrips_[stripIndex][deviceIndex] = -1.0f;
            } else if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
                relStart_ehStrips_[stripIndex][deviceIndex] = ehStrips_[stripIndex][deviceIndex];
            }

            for (int i = 0; i < 3; i++) {
                if (!stripOff) {
                    createStripAbsolute(deviceIndex, stripIndex, i, deviceLookups, sink, eventTime, voiceRouter);
                }
                createStripRelative(deviceIndex, stripIndex, i, deviceLookups, sink, eventTime, voiceRouter);
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

void MidiService::processNoteKey(const osc::Message& oscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy) {
    state->messageCount++;
    state->lastTimestamp = oscMsg.timestamp;

    if (!oscMsg.active) {
        createNoteOff(keyLookup, state, sink, eventTime, voiceRouter);
    } else if (state->status == KeyStatus::Off) {
        state->status = KeyStatus::Pending;
    } else if (state->messageCount == PRESSURE_HISTORY_LENGTH && state->status == KeyStatus::Pending) {
        createNoteOn(keyLookup, state, sink, eventTime, voiceRouter, expressionPolicy);
    } else if (state->status == KeyStatus::Active) {
        const bool shouldEmit = expressionPolicy ? expressionPolicy->shouldEmitContinuousUpdate(state->messageCount)
                                                 : state->messageCount >= 64;
        if (shouldEmit) {
            createNoteHold(keyLookup, state, sink, eventTime, voiceRouter, expressionPolicy);
        }
    }
}

void MidiService::processCmdKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter) {
    if (!oscMsg.active) {
        if (keyLookup.cmdType == 2) // Momentary
            createMidiMsgOff(keyLookup, state, sink, outgoingOscMsg, oscMsg.devId, eventTime, voiceRouter);
    } else if (state->status == KeyStatus::Off) {
        if (keyLookup.cmdType == 1 && state->isLatchOn)
            createMidiMsgOff(keyLookup, state, sink, outgoingOscMsg, oscMsg.devId, eventTime, voiceRouter);
        else
            createMidiMsgOn(keyLookup, state, sink, outgoingOscMsg, oscMsg.devId, eventTime, voiceRouter);
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

    const bool hasDirectMidi2Output = std::any_of(directUmpOutputs_.begin(), directUmpOutputs_.end(), [](const auto& output) {
        return output.isAlive();
    });

    if ((isVirtualTarget_ && isMidi2Mode_ && hasDirectMidi2Output) || umpOutput_.isAlive())
    {
        drainDirectUMPs(pendingMidiBuffer_);
    }
    else
    {
        buffer.addEvents(pendingMidiBuffer_, 0, -1, eventTime);
    }
    
    pendingMidiBuffer_.clear();
}

void MidiService::sendMidiBufferToOutput(const juce::MidiBuffer& buffer, juce::MidiOutput* output) const {
    if (output == nullptr || buffer.isEmpty())
        return;

    output->sendBlockOfMessagesNow(buffer);
}

void MidiService::sendMidiBufferToDistinctOutputs(const juce::MidiBuffer& buffer,
                                                  const std::array<juce::MidiOutput*, 3>& outputs,
                                                  juce::MidiOutput* fallbackOutput) const {
    if (buffer.isEmpty())
        return;

    std::set<juce::MidiOutput*> sentOutputs;
    for (auto* output : outputs) {
        if (output != nullptr && sentOutputs.insert(output).second)
            sendMidiBufferToOutput(buffer, output);
    }

    if (fallbackOutput != nullptr && sentOutputs.insert(fallbackOutput).second)
        sendMidiBufferToOutput(buffer, fallbackOutput);
}

void MidiService::drainDirectUMPs(juce::MidiBuffer& buffer, bool silentIfFailed)
{
    const juce::ScopedLock sl(umpOutputLock_);

    const bool useDirectMidi2Outputs = isVirtualTarget_ && isMidi2Mode_;
    const bool hasDirectMidi2Output = std::any_of(directUmpOutputs_.begin(), directUmpOutputs_.end(), [](const auto& output) {
        return output.isAlive();
    });
    const bool hasTargetOutput = useDirectMidi2Outputs ? hasDirectMidi2Output : umpOutput_.isAlive();

    if (hasTargetOutput)
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

                    if (useDirectMidi2Outputs)
                    {
                        const auto group = juce::universal_midi_packets::Utils::getGroup(data[0]);
                        const auto zoneIndex = group < directUmpOutputs_.size() ? static_cast<size_t>(group) : size_t { 0 };
                        auto& output = directUmpOutputs_[zoneIndex];
                        if (output.isAlive())
                            output.send(begin, end);
                    }
                    else if (umpOutput_.isAlive())
                    {
                        umpOutput_.send(begin, end);
                    }
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
                        if (umpOutput_.isAlive())
                            umpOutput_.send(begin, end);
                    });
                }
                else
                {
                    juce::Logger::writeToLog("MidiService: Skipping message of size " + juce::String((int)size) + " Status=0x" + juce::String::toHexString(size > 0 ? (int)data[0] : 0) + " in MIDI 1.0 mode (likely stale UMP data).");
                }
            }
        }
    }
    else if (!buffer.isEmpty() && !silentIfFailed)
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
            auto zoneIndex = 0;
            if (name.containsIgnoreCase("Zone 2"))
                zoneIndex = 1;
            else if (name.containsIgnoreCase("Zone 3"))
                zoneIndex = 2;

            if (virtualEndpoints_[static_cast<size_t>(zoneIndex)].isAlive())
                return virtualEndpoints_[static_cast<size_t>(zoneIndex)].getId();
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
        const bool canUseInternalDirectOutput = isMidi2Mode_ && isFirstInstance_ && std::all_of(virtualEndpoints_.begin(), virtualEndpoints_.end(), [](const auto& endpoint) {
            return endpoint.isAlive();
        });

        if (canUseInternalDirectOutput)
        {
            midiOutputName_ = "ECMapper Direct UMP";
            isVirtualTarget_ = true;
            lastEndpointId_ = {};
            umpOutput_ = {};

            if (!umpSession_.has_value())
                umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");

            if (umpSession_.has_value())
                for (size_t zoneIndex = 0; zoneIndex < directUmpOutputs_.size(); ++zoneIndex)
                    if (!directUmpOutputs_[zoneIndex].isAlive())
                        directUmpOutputs_[zoneIndex] = umpSession_->connectOutput(virtualEndpoints_[zoneIndex].getId());

            juce::Logger::writeToLog("MidiService: No MIDI output selected. Falling back to internal direct output.");
            return;
        }

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

        if (isMidi2Mode_ && isFirstInstance_ && umpSession_.has_value())
            for (size_t zoneIndex = 0; zoneIndex < directUmpOutputs_.size(); ++zoneIndex)
                if (!directUmpOutputs_[zoneIndex].isAlive() && virtualEndpoints_[zoneIndex].isAlive())
                    directUmpOutputs_[zoneIndex] = umpSession_->connectOutput(virtualEndpoints_[zoneIndex].getId());

        return;
    }

    if (!umpSession_.has_value() || !umpOutput_.isAlive())
    {
        if (!umpSession_.has_value())
            umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");
            
        juce::Logger::writeToLog("MidiService: Attempting to connect UMP output/input to ID: src='" + endpointId.src + "', dst='" + endpointId.dst + "'");
        
        if (endpointId.dst.isNotEmpty())
        {
            umpOutput_ = (*umpSession_).connectOutput(endpointId);
            
            if (umpOutput_.isAlive())
                juce::Logger::writeToLog("MidiService: Connected direct UMP output to " + name);
            else
                juce::Logger::writeToLog("MidiService: Failed to connect direct UMP output to " + name + ". Will retry if endpoints change.");
        }
        
        if (endpointId.src.isNotEmpty())
        {
            if (umpInput_.isAlive())
                umpInput_.removeConsumer(*this);
                
            umpInput_ = (*umpSession_).connectInput(endpointId, isMidi2Mode_ ? juce::universal_midi_packets::PacketProtocol::MIDI_2_0 : juce::universal_midi_packets::PacketProtocol::MIDI_1_0);
            
            if (umpInput_.isAlive())
            {
                juce::Logger::writeToLog("MidiService: Connected direct UMP input to " + name);
                umpInput_.addConsumer(*this);
            }
            else
            {
                juce::Logger::writeToLog("MidiService: Failed to connect direct UMP input to " + name);
            }
        }
            
        sendIdentification();
    }
}

void MidiService::setStandaloneLegacyMidiOutputs(const std::array<juce::MidiOutput*, 3>& outputs, juce::MidiOutput* defaultOutput)
{
    const juce::ScopedLock sl(standaloneLegacyOutputLock_);
    standaloneLegacyOutputs_ = outputs;
    standaloneDefaultLegacyOutput_ = defaultOutput;
}

bool MidiService::isStandaloneLegacyZoneRoutingEnabled() const
{
    if (isMidi2Mode_)
        return false;

    const juce::ScopedLock sl(standaloneLegacyOutputLock_);
    return std::any_of(standaloneLegacyOutputs_.begin(), standaloneLegacyOutputs_.end(), [](juce::MidiOutput* output) {
        return output != nullptr;
    });
}

void MidiService::sendStandaloneLegacyMidiBuffers(const juce::MidiBuffer& sharedBuffer, const std::array<juce::MidiBuffer, 3>& zoneBuffers)
{
    std::array<juce::MidiOutput*, 3> outputs {};
    juce::MidiOutput* fallbackOutput = nullptr;
    {
        const juce::ScopedLock sl(standaloneLegacyOutputLock_);
        outputs = standaloneLegacyOutputs_;
        fallbackOutput = standaloneDefaultLegacyOutput_;
    }

    sendMidiBufferToDistinctOutputs(sharedBuffer, outputs, fallbackOutput);

    for (size_t i = 0; i < zoneBuffers.size(); ++i) {
        auto* output = outputs[i] != nullptr ? outputs[i] : fallbackOutput;
        sendMidiBufferToOutput(zoneBuffers[i], output);
    }
}

void MidiService::endpointsChanged()
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (isVirtualTarget_) return;

    bool needsOutput = lastEndpointId_.dst.isNotEmpty() && !umpOutput_.isAlive();
    bool needsInput = lastEndpointId_.src.isNotEmpty() && !umpInput_.isAlive();

    if (needsOutput || needsInput)
    {
        if (!umpSession_.has_value())
            umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");

        if (needsOutput)
            umpOutput_ = (*umpSession_).connectOutput(lastEndpointId_);

        if (needsInput)
        {
            umpInput_ = (*umpSession_).connectInput(lastEndpointId_, isMidi2Mode_ ? juce::universal_midi_packets::PacketProtocol::MIDI_2_0 : juce::universal_midi_packets::PacketProtocol::MIDI_1_0);
            if (umpInput_.isAlive())
                umpInput_.addConsumer(*this);
        }

        bool outputOk = lastEndpointId_.dst.isEmpty() || umpOutput_.isAlive();
        bool inputOk = lastEndpointId_.src.isEmpty() || umpInput_.isAlive();

        if (outputOk && inputOk)
            juce::Logger::writeToLog("MidiService: Automatically connected direct UMP port(s) to " + midiOutputName_ + " after endpoint change.");
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

        const auto mt = juce::universal_midi_packets::Utils::getMessageType (view[0]);
        const auto group = juce::universal_midi_packets::Utils::getGroup (view[0]);

        if (mt == juce::universal_midi_packets::Utils::MessageKind::channelVoice2)
        {
            if (isMidi2Mode_ && !remoteSupportsPerNote_)
            {
                juce::Logger::writeToLog("MidiService: MIDI 2.0 traffic detected from remote. Enabling per-note expression.");
                remoteSupportsPerNote_ = true;
                if (voiceRouter_) voiceRouter_->setRemoteSupportsPerNote(true);
                logMidiExpressionMode();
            }
        }
        else if (mt != juce::universal_midi_packets::Utils::MessageKind::channelVoice1)
        {
            juce::Logger::writeToLog("MidiService: [UMP RECV] MT:0x" + juce::String::toHexString((int)mt) + 
                                     " Group:" + juce::String((int)group) + 
                                     " W0:0x" + juce::String::toHexString((int)view[0]));
        }

        if (ciDevice_) {
            dispatcher_.dispatch(*it, time, [this](BytesOnGroup msg, double) {
                std::vector<std::byte> bytesCopy(msg.bytes.data(), msg.bytes.data() + msg.bytes.size());
                juce::MessageManager::callAsync([weakThis = juce::WeakReference<MidiService>(this), g = msg.group, bytesData = std::move(bytesCopy)]() mutable {
                    if (auto* service = weakThis.get()) {
                        if (!service->isInitialized()) return;
                        
                        juce::Span<const std::byte> bytes(bytesData.data(), bytesData.size());
                        if (bytes.size() >= 4 && bytes.front() == std::byte{0xf0} && bytes.back() == std::byte{0xf7}) {
                            auto incomingDeviceID = bytes[2];
                            auto payload = juce::Span<const std::byte>(bytes.data() + 1, bytes.size() - 2);

                            if (payload.size() >= 4 && payload[0] == std::byte{0x7e}) {
                                if (payload[2] == std::byte{0x0d}) {
                                    juce::Logger::writeToLog("MidiService: Received MIDI-CI SysEx. SubID2=0x" + juce::String::toHexString((int)payload[3]));
                                }

                                // Identity Request
                                if (payload[2] == std::byte{0x06} && payload[3] == std::byte{0x01}) {
                                    juce::Logger::writeToLog("MidiService: Received Identity Request. Replying.");
                                    service->sendIdentityResponse(g, incomingDeviceID);
                                    return;
                                }

                                // MIDI-CI
                                if (payload[2] == std::byte{0x0d} && payload.size() >= 13) {
                                    auto subID2 = payload[3];
                                    auto src = (uint32_t)payload[5] | ((uint32_t)payload[6] << 7) | ((uint32_t)payload[7] << 14) | ((uint32_t)payload[8] << 21);
                                    auto sourceMUID = juce::midi_ci::MUID::makeUnchecked(src);

                                    juce::Logger::writeToLog("MidiService: MIDI-CI SubID2=0x" + juce::String::toHexString((int)subID2) + 
                                                             " from MUID=0x" + juce::String::toHexString(sourceMUID.get()));

                                    if (subID2 == std::byte{0x10}) { // Initiate Protocol Negotiation
                                        juce::Logger::writeToLog("MidiService: Received Initiate Protocol Negotiation. Replying with MIDI 2.0.");
                                        std::vector<std::byte> body;
                                        body.push_back(std::byte{0x30}); // Authority Level
                                        body.push_back(std::byte{0x01}); // Number of selected protocols
                                        body.push_back(std::byte{0x02}); // MIDI 2.0
                                        body.push_back(std::byte{0x00}); body.push_back(std::byte{0x00}); body.push_back(std::byte{0x00}); body.push_back(std::byte{0x00});
                                        service->sendCISysex(g, sourceMUID, std::byte{0x11}, juce::Span<const std::byte>(body.data(), body.size()), incomingDeviceID);
                                        
                                        if (service->isMidi2Mode_ && !service->remoteSupportsPerNote_) {
                                            service->remoteSupportsPerNote_ = true;
                                            if (service->voiceRouter_) service->voiceRouter_->setRemoteSupportsPerNote(true);
                                            service->logMidiExpressionMode();
                                        }
                                        return;
                                    }
                                    else if (subID2 == std::byte{0x11}) { // Reply to Initiate Protocol Negotiation
                                        if (payload.size() >= 18 && payload[15] == std::byte{0x02}) {
                                            juce::Logger::writeToLog("MidiService: Remote accepted MIDI 2.0 protocol.");
                                            if (service->isMidi2Mode_ && !service->remoteSupportsPerNote_) {
                                                service->remoteSupportsPerNote_ = true;
                                                if (service->voiceRouter_) service->voiceRouter_->setRemoteSupportsPerNote(true);
                                                service->logMidiExpressionMode();
                                            }
                                        }
                                        return;
                                    }
                                    else if (subID2 == std::byte{0x12}) { // Set New Protocol
                                        if (payload.size() >= 18 && payload[13] == std::byte{0x02}) {
                                            juce::Logger::writeToLog("MidiService: Protocol confirmed: MIDI 2.0");
                                            if (service->isMidi2Mode_ && !service->remoteSupportsPerNote_) {
                                                service->remoteSupportsPerNote_ = true;
                                                if (service->voiceRouter_) service->voiceRouter_->setRemoteSupportsPerNote(true);
                                                service->logMidiExpressionMode();
                                            }
                                        }
                                        return;
                                    }
                                    else if (subID2 == std::byte{0x13}) { // Test New Protocol
                                        service->sendCISysex(g, sourceMUID, std::byte{0x14}, {}, incomingDeviceID);
                                        return;
                                    }
                                }
                            }

                            if (service->ciDevice_) {
                                service->ciDevice_->processMessage({ g, payload });
                            }
                        }
                    }
                });
            });
        }

        if (universal_midi_packets::Utils::getMessageType (view[0]) == universal_midi_packets::Utils::MessageKind::stream)
        {
            auto status = universal_midi_packets::Utils::U8<1>::get (view[0]);
            juce::Logger::writeToLog("MidiService: Received Stream Message 0x" + juce::String::toHexString((int)status) + 
                                     " size=" + juce::String(view.size()));
            
            if (status == 0x01 && view.size() > 1) // Endpoint Info Notification
            {
                bool supportsMidi2 = (view[1] & (1 << 9)) != 0;
                if (supportsMidi2 && isMidi2Mode_ && !remoteSupportsPerNote_)
                {
                    juce::Logger::writeToLog("MidiService: Endpoint Info indicates MIDI 2.0 support. Enabling per-note expression.");
                    remoteSupportsPerNote_ = true;
                    if (voiceRouter_) voiceRouter_->setRemoteSupportsPerNote(true);
                    logMidiExpressionMode();
                }
            }

            if (status == 0x00 || status == 0x05 || status == 0x10) // Endpoint Discovery, Stream Config Request, or Function Block Discovery
            {
                shouldRespond = true;
                
                if (status == 0x05 && view.size() > 1)
                {
                    auto requestedProtocol = (view[1] >> 24) & 0xFF;
                    juce::Logger::writeToLog("MidiService: Host requested protocol 0x" + juce::String::toHexString((int)requestedProtocol));
                    
                    if (requestedProtocol == 2 && isMidi2Mode_ && !remoteSupportsPerNote_)
                    {
                        juce::Logger::writeToLog("MidiService: Host requested MIDI 2.0 protocol. Enabling per-note expression.");
                        remoteSupportsPerNote_ = true;
                        if (voiceRouter_) voiceRouter_->setRemoteSupportsPerNote(true);
                        logMidiExpressionMode();
                    }
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
    const auto clearMidi2VirtualOutputs = [this]() {
        for (auto& virtualUmpInput : virtualUmpInputs_)
        {
            if (virtualUmpInput.isAlive())
                virtualUmpInput.removeConsumer(*this);

            virtualUmpInput = {};
        }

        for (auto& directUmpOutput : directUmpOutputs_)
            directUmpOutput = {};

        for (auto& virtualEndpoint : virtualEndpoints_)
            virtualEndpoint = {};
    };

    if (virtualMidiLock_ == nullptr)
    {
        juce::Logger::writeToLog("MidiService: Initializing InterProcessLock for virtual MIDI.");
        virtualMidiLock_ = std::make_unique<juce::InterProcessLock>("ECMapper_VirtualMidi_Lock");
    }

    const juce::ScopedLock sl(umpOutputLock_);

    if (!isMidi2Mode_)
    {
        clearMidi2VirtualOutputs();
        isVirtualTarget_ = false;

        if (virtualUmpOutput_.has_value() && *virtualUmpOutput_)
        {
            juce::Logger::writeToLog("MidiService: Closing legacy virtual output.");
            virtualUmpOutput_ = {};
            virtualUmpInputMirror_ = {};
        }

        if (virtualMidiLock_ != nullptr && isFirstInstance_)
        {
            virtualMidiLock_->exit();
            isFirstInstance_ = false;
        }

        return;
    }

    if (!isFirstInstance_)
        isFirstInstance_ = virtualMidiLock_->enter(0);

    if (!isFirstInstance_)
    {
        juce::Logger::writeToLog("MidiService: Another instance manages the Direct virtual MIDI output.");
        return;
    }

    juce::Logger::writeToLog("MidiService: This is the first instance. Managing virtual port.");

    if (virtualUmpOutput_.has_value() && *virtualUmpOutput_)
    {
        juce::Logger::writeToLog("MidiService: Closing legacy virtual output.");
        virtualUmpOutput_ = {};
        virtualUmpInputMirror_ = {};
    }

    const bool needsCreation = std::any_of(virtualEndpoints_.begin(), virtualEndpoints_.end(), [](const auto& endpoint) {
        return !endpoint.isAlive();
    });

    if (!needsCreation)
    {
        juce::Logger::writeToLog("MidiService: Virtual MIDI 2.0 ports already exist.");
        return;
    }

    if (!umpSession_.has_value())
    {
        juce::Logger::writeToLog("MidiService: Creating UMP session.");
        umpSession_ = juce::universal_midi_packets::Endpoints::getInstance()->makeSession("ECMapperUMP");
    }

    if (!umpSession_.has_value())
    {
        juce::Logger::writeToLog("MidiService: Failed to create UMP session.");
        return;
    }

    using namespace juce::universal_midi_packets;

    clearMidi2VirtualOutputs();

    DeviceInfo devInfo;
    devInfo.manufacturer = { std::byte { 0x7D }, std::byte { 0x00 }, std::byte { 0x00 } };
    devInfo.family = { std::byte { 0x01 }, std::byte { 0x00 } };
    devInfo.modelNumber = { std::byte { 0x01 }, std::byte { 0x00 } };
    devInfo.revision = { std::byte { 0x01 }, std::byte { 0x00 }, std::byte { 0x00 }, std::byte { 0x00 } };

    for (size_t zoneIndex = 0; zoneIndex < virtualEndpoints_.size(); ++zoneIndex)
    {
        const auto zoneNumber = static_cast<int>(zoneIndex) + 1;
        juce::Logger::writeToLog("MidiService: Creating VirtualEndpoint (MIDI 2.0) for Zone " + juce::String(zoneNumber) + ".");

        const auto blocks = std::array {
            Block {}.withName("Zone " + juce::String(zoneNumber))
                    .withDirection(BlockDirection::bidirectional)
                    .withUiHint(BlockUiHint::bidirectional)
                    .withEnabled(true)
                    .withFirstGroup(static_cast<uint8_t>(zoneIndex))
                    .withNumGroups(1)
                    .withMIDI1ProxyKind(BlockMIDI1ProxyKind::inapplicable)
        };

        virtualEndpoints_[zoneIndex] = umpSession_->createVirtualEndpoint("ECMapper Direct UMP Zone " + juce::String(zoneNumber),
                                                                           devInfo,
                                                                           "ECMapper-Direct-UMP-Zone-" + juce::String(zoneNumber),
                                                                           PacketProtocol::MIDI_2_0,
                                                                           blocks,
                                                                           BlocksAreStatic::yes);

        if (!virtualEndpoints_[zoneIndex].isAlive())
        {
            juce::Logger::writeToLog("MidiService: Failed to create virtual MIDI 2.0 endpoint for Zone " + juce::String(zoneNumber) + ".");
            continue;
        }

        juce::Logger::writeToLog("MidiService: Successfully created virtual MIDI 2.0 endpoint for Zone " + juce::String(zoneNumber) + ".");
        directUmpOutputs_[zoneIndex] = umpSession_->connectOutput(virtualEndpoints_[zoneIndex].getId());

        virtualUmpInputs_[zoneIndex] = umpSession_->connectInput(virtualEndpoints_[zoneIndex].getId(), PacketProtocol::MIDI_2_0);
        if (virtualUmpInputs_[zoneIndex].isAlive())
            virtualUmpInputs_[zoneIndex].addConsumer(*this);
    }

    sendIdentification();
    endpointsChanged();
}

bool MidiService::isVirtualOutputActive() const
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (!isMidi2Mode_)
        return false;

    return std::any_of(virtualEndpoints_.begin(), virtualEndpoints_.end(), [](const auto& endpoint) {
        return endpoint.isAlive();
    });
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

void MidiService::reduceBreath(juce::MidiBuffer& buffer, int eventTime, int blockSampleCount) {
    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    MidiBufferPerformanceEventSink sink { buffer, snapshot->protocol.get() };
    reduceBreath(buffer, sink, eventTime, blockSampleCount);
}

void MidiService::reduceBreath(juce::MidiBuffer&, PerformanceEventSink& sink, int eventTime, int blockSampleCount) {
    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    if (snapshot == nullptr)
        return;

    MidiVoiceRouter* voiceRouter = snapshot->voiceRouter.get();
    const auto& runtimeLookups = snapshot->configLookups;
    const int effectiveBlockSamples = std::max(1, blockSampleCount);
    for (int i = 0; i < 3; i++) {
        if (ehBreath_[i] <= 0.0f) continue;

        breathSamplesSinceUpdate_[i] += effectiveBlockSamples;
        if (breathSamplesSinceUpdate_[i] <= BREATH_STABILITY_HOLD_SAMPLES)
            continue;

        const int decaySamples = std::min(effectiveBlockSamples, breathSamplesSinceUpdate_[i] - BREATH_STABILITY_HOLD_SAMPLES);
        const float decayAmount = static_cast<float>(decaySamples) * BREATH_RELEASE_PER_SAMPLE;
        ehBreath_[i] = (ehBreath_[i] > breathZeroThreshold_[i]) ? std::max(0.0f, ehBreath_[i] - decayAmount) : 0.0f;
        createBreath(i, runtimeLookups[i], sink, eventTime, voiceRouter);
    }
}

void MidiService::createBreath(int deviceIndex, const ConfigLookup& keyLookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter) {
    float val = (ehBreath_[deviceIndex] < breathZeroThreshold_[deviceIndex]) 
                       ? 0.0f : ehBreath_[deviceIndex] - breathZeroThreshold_[deviceIndex];
    
    // Scale val to [0, 1] range after threshold
    if (val > 0.0f) {
        val = val / (1.0f - breathZeroThreshold_[deviceIndex]);
    }

    for (int z = 0; z < 3; ++z) {
        addMidiValueMessage(static_cast<InstrumentType>(deviceIndex + 1), keyLookup.breath[z].channel, val * 3.0f, keyLookup.breath[z].midiValue, keyLookup.breath[z].pbRange, 0, sink, false, ExpressionCurveTarget::Breath, eventTime, voiceRouter, z);
    }
}

void MidiService::createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    addStripValueMessage(static_cast<InstrumentType>(deviceIndex + 1), strip.channel, ehStrips_[stripIndex][deviceIndex], strip.absMidiValue, strip.pbRange, sink, false, eventTime, voiceRouter, zoneIndex);
}

void MidiService::createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter) {
    auto& strip = (stripIndex == 0) ? keyLookup.strip1[zoneIndex] : keyLookup.strip2[zoneIndex];
    float relValue = (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) 
                   ? 0.0f : relStart_ehStrips_[stripIndex][deviceIndex] - ehStrips_[stripIndex][deviceIndex];
    
    if (relStart_ehStrips_[stripIndex][deviceIndex] < 0.0f) {
        currentStripPBperChannel_[strip.channel > 0 ? strip.channel - 1 : 0] = 0;
    }

    addStripValueMessage(static_cast<InstrumentType>(deviceIndex + 1), strip.channel, relValue, strip.relMidiValue, strip.pbRange, sink, true, eventTime, voiceRouter, zoneIndex);
}

void MidiService::createNoteOn(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy) {
    int deviceIndex = static_cast<int>(keyLookup.keyId.deviceType) - 1;
    int totalTranspose = (deviceIndex >= 0 && deviceIndex < 3) ? (latchTranspose_[deviceIndex] + momentaryTranspose_[deviceIndex]) : 0;
    const int zoneIndex = zoneIndexFromKeyId(keyLookup.keyId);

    state->midiChannel = voiceRouter ? voiceRouter->findMidiChannelForNewNote(keyLookup.output, keyLookup.notes[0]) : static_cast<int>(keyLookup.output);

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

    state->noteOnTimestamp = state->lastTimestamp;
    createNoteHold(keyLookup, state, sink, eventTime, voiceRouter, expressionPolicy);
    float vel = calculateNoteOnVelocity(keyLookup.keyId.deviceType, state);
    
    for (int i = 0; i < 4; i++) {
        int noteNo = state->activeNotes[i];
        if (noteNo > -1) {
            if (countPlayingNoteMatches(state->midiChannel, noteNo) == 0) {
                sink.pushEvent(PerformanceEvent::noteOn(state->midiChannel, noteNo, vel, eventTime, zoneIndex));
                juce::Logger::writeToLog("MidiService: Note On - Chan: " + juce::String(state->midiChannel) + 
                                         ", Note: " + juce::String(noteNo) + 
                                         ", Velocity: " + juce::String(vel, 3));
            }
            playingNotes_.push_back({state->midiChannel, noteNo});
        }
    }
    state->status = KeyStatus::Active;
}

void MidiService::createNoteOff(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter) {
    int channel = state->midiChannel;
    const int zoneIndex = zoneIndexFromKeyId(keyLookup.keyId);
    if (voiceRouter) voiceRouter->releaseMidiChannel(keyLookup.output, keyLookup.notes[0], channel);

    if (channel > 0 && channel <= 16) {
        chanNotePri_[channel - 1].remove_if([&keyLookup](const LayoutWrapper::KeyId& id) { return id == keyLookup.keyId; });
    }

    float vel = calculateNoteOffVelocity(keyLookup.keyId.deviceType, state);
    for (int i = 0; i < 4; i++) {
        int noteToTurnOff = state->activeNotes[i];
        if (noteToTurnOff > -1) {
            if (countPlayingNoteMatches(channel, noteToTurnOff) < 2) {
                sink.pushEvent(PerformanceEvent::noteOff(channel, noteToTurnOff, vel, eventTime, zoneIndex));
                juce::Logger::writeToLog("MidiService: Note Off - Chan: " + juce::String(channel) + 
                                         ", Note: " + juce::String(noteToTurnOff) + 
                                         ", Velocity: " + juce::String(vel, 3));
            }
            removeOneNoteMatch(channel, noteToTurnOff);
            state->activeNotes[i] = -1;
        }
    }
    
    if (channel > 0 && channel <= 16 && chanNotePri_[channel - 1].empty()) {
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.pressure, keyLookup.pbRange, keyLookup.notes[0], sink, false, ExpressionCurveTarget::Pressure, eventTime, voiceRouter);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.roll, keyLookup.pbRange, keyLookup.notes[0], sink, true, ExpressionCurveTarget::Roll, eventTime, voiceRouter);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, 0, keyLookup.yaw, keyLookup.pbRange, keyLookup.notes[0], sink, true, ExpressionCurveTarget::Yaw, eventTime, voiceRouter);
    }
    state->status = KeyStatus::Off;
    state->messageCount = 0;
    state->noteOnTimestamp = 0;
    state->lastTimestamp = 0;
}

void MidiService::createMidiMsgOn(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiVoiceRouter* voiceRouter) {
    state->isLatchOn = true;
    const int zoneIndex = zoneIndexFromKeyId(keyLookup.keyId);
    state->midiChannel = voiceRouter ? voiceRouter->findMidiChannelForNewNote(keyLookup.output, -1) : 
                         ((keyLookup.output == MidiChannelType::MPE_Low) ? 1 : 
                          (keyLookup.output == MidiChannelType::MPE_High) ? 16 : static_cast<int>(keyLookup.output));

    if (keyLookup.msgType == 4) {
        createAllNotesOff(sink, eventTime);
    } else if (keyLookup.msgType == 1) {
        sink.pushEvent(PerformanceEvent::controllerChange(state->midiChannel, -1, keyLookup.cmdCC, keyLookup.cmdOn / 127.0f, false, eventTime, zoneIndex));
    } else if (keyLookup.msgType == 2) {
        sink.pushEvent(PerformanceEvent::programChange(state->midiChannel, keyLookup.cmdOn, eventTime, zoneIndex));
    } else if (keyLookup.msgType == 3) {
        if (keyLookup.cmdOn == 1) sink.pushEvent(PerformanceEvent::midiStart(eventTime, zoneIndex));
        else if (keyLookup.cmdOn == 2) sink.pushEvent(PerformanceEvent::midiStop(eventTime, zoneIndex));
        else if (keyLookup.cmdOn == 3) sink.pushEvent(PerformanceEvent::midiContinue(eventTime, zoneIndex));
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

void MidiService::createMidiMsgOff(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiVoiceRouter* voiceRouter) {
    juce::ignoreUnused(voiceRouter);
    const int zoneIndex = zoneIndexFromKeyId(keyLookup.keyId);
    if (keyLookup.cmdType != 3) { // Not Trigger
        if (keyLookup.msgType == 4) {
            createAllNotesOff(sink, eventTime);
        } else if (keyLookup.msgType == 1) {
            sink.pushEvent(PerformanceEvent::controllerChange(state->midiChannel, -1, keyLookup.cmdCC, keyLookup.cmdOff / 127.0f, false, eventTime, zoneIndex));
        } else if (keyLookup.msgType == 2) {
            sink.pushEvent(PerformanceEvent::programChange(state->midiChannel, keyLookup.cmdOff, eventTime, zoneIndex));
        } else if (keyLookup.msgType == 3) {
            if (keyLookup.cmdOff == 1) sink.pushEvent(PerformanceEvent::midiStart(eventTime, zoneIndex));
            else if (keyLookup.cmdOff == 2) sink.pushEvent(PerformanceEvent::midiStop(eventTime, zoneIndex));
            else if (keyLookup.cmdOff == 3) sink.pushEvent(PerformanceEvent::midiContinue(eventTime, zoneIndex));
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

void MidiService::createAllNotesOff(PerformanceEventSink& sink, int eventTime) {
    juce::Logger::writeToLog("MidiService: Sending All Notes Off to all channels.");
    for (int i = 1; i <= 16; i++) {
        sink.pushEvent(PerformanceEvent::allNotesOff(i, eventTime));
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
    MidiVoiceRouter* voiceRouter = voiceRouter_.get();
    MidiBufferPerformanceEventSink sink { localMessages, protocol };

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
                    const int noteNumber = keyState.activeNotes[i];
                    if (noteNumber > -1) {
                        if (countPlayingNoteMatches(channel, noteNumber) < 2) {
                            sink.pushEvent(PerformanceEvent::noteOff(channel, noteNumber, vel, 0));
                            juce::Logger::writeToLog("MidiService: Note Off (Flush) - Chan: " + juce::String(channel) + 
                                                     ", Note: " + juce::String(noteNumber));
                        }
                        removeOneNoteMatch(channel, noteNumber);
                        keyState.activeNotes[i] = -1;
                    }
                }

                if (voiceRouter)
                    voiceRouter->releaseMidiChannel(keyLookup.output, keyLookup.notes[0], channel);

                if (channel > 0 && channel <= 16) {
                    chanNotePri_[channel - 1].remove_if([&keyId](const LayoutWrapper::KeyId& id) { return id == keyId; });

                    if (chanNotePri_[channel - 1].empty()) {
                        currentKeyPBperChannel_[channel - 1] = 0.0f;
                        currentStripPBperChannel_[channel - 1] = 0.0f;
                        sink.pushEvent(PerformanceEvent::channelPressure(channel, -1, 0.0f, false, 0));
                        sink.pushEvent(PerformanceEvent::pitchBend(channel, -1, 0.5f, false, 0));
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

void MidiService::createNoteHold(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy) {
    int channel = state->midiChannel;
    const int zoneIndex = zoneIndexFromKeyId(keyLookup.keyId);
    if (channel > 0 && channel <= 16 && (isMidi2Mode_ || chanNotePri_[channel - 1].empty() || chanNotePri_[channel - 1].front() == keyLookup.keyId)) {
        const float transitionedRoll = applyNoteOnTransition(*state, state->ehRoll, expressionPolicy);
        const float transitionedYaw = applyNoteOnTransition(*state, state->ehYaw, expressionPolicy);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, transitionedRoll, keyLookup.roll, keyLookup.pbRange, state->activeNotes[0], sink, true, ExpressionCurveTarget::Roll, eventTime, voiceRouter, zoneIndex);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, transitionedYaw, keyLookup.yaw, keyLookup.pbRange, state->activeNotes[0], sink, true, ExpressionCurveTarget::Yaw, eventTime, voiceRouter, zoneIndex);
        addMidiValueMessage(keyLookup.keyId.deviceType, channel, state->ehPressureHistory.back(), keyLookup.pressure, keyLookup.pbRange, state->activeNotes[0], sink, false, ExpressionCurveTarget::Pressure, eventTime, voiceRouter, zoneIndex);
    }
    state->messageCount = 0;
}

float MidiService::applyNoteOnTransition(const KeyState& state, float measuredValue, ExpressionEmissionPolicy* expressionPolicy) const {
    const auto* policy = expressionPolicy != nullptr ? expressionPolicy : expressionPolicy_.get();
    const int transitionMs = policy != nullptr ? policy->getConfig().noteOnTransitionMilliseconds : 0;
    if (transitionMs <= 0 || state.noteOnTimestamp == 0 || state.lastTimestamp <= state.noteOnTimestamp)
        return state.noteOnTimestamp == 0 ? measuredValue : 0.0f;

    const uint64_t transitionDurationUs = static_cast<uint64_t>(transitionMs) * 1000ULL;
    const uint64_t elapsedUs = state.lastTimestamp - state.noteOnTimestamp;
    if (elapsedUs >= transitionDurationUs)
        return measuredValue;

    const float progress = static_cast<float>(elapsedUs) / static_cast<float>(transitionDurationUs);
    return measuredValue * progress;
}

void MidiService::addMidiValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, PerformanceEventSink& sink, bool isBipolar, ExpressionCurveTarget curveTarget, int eventTime, MidiVoiceRouter* voiceRouter, int zoneIndex) {
    if (midiValue.valueType == MidiValueType::Off) return;
    
    int resolvedChannel = channel;
    if (resolvedChannel > 16 && voiceRouter) {
        resolvedChannel = voiceRouter->findMidiChannelForNewNote(static_cast<MidiChannelType>(channel), noteNo);
    }
    
    if (resolvedChannel < 1 || resolvedChannel > 16) return;
    
    int deviceIndex = static_cast<int>(deviceType) - 1;
    float gain = isBipolar ? 1.7f : 1.0f;
    if (deviceIndex >= 0 && deviceIndex < 3) {
        if (curveTarget == ExpressionCurveTarget::Yaw) gain = yawSensitivity_[deviceIndex];
        else if (curveTarget == ExpressionCurveTarget::Roll) gain = rollSensitivity_[deviceIndex];
        else if (curveTarget == ExpressionCurveTarget::Pressure) gain = pressureSensitivity_[deviceIndex];
        else if (curveTarget == ExpressionCurveTarget::Breath) gain = breathSensitivity_[deviceIndex];
    }

    float normalized = isBipolar ? (std::clamp(ehValue * gain, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    if (curveTarget == ExpressionCurveTarget::Roll)
        normalized = applyRollPreCurve(normalized);
    normalized = applyExpressionCurve(deviceType, curveTarget, normalized, isBipolar);
    
    bool useNativePerNote = isMidi2Mode_ && remoteSupportsPerNote_ && noteNo != -1;

    if (midiValue.valueType == MidiValueType::Pitchbend) {
        float notePB = calculatePitchBendCurve(normalized) * pbRange;
        
        if (useNativePerNote) {
            float protocolValue = notePB * 0.5f + 0.5f;
            sink.pushEvent(PerformanceEvent::pitchBend(resolvedChannel, noteNo, protocolValue, true, eventTime, zoneIndex));
        } else {
            currentKeyPBperChannel_[resolvedChannel - 1] = notePB;
            float totalPB = std::clamp(currentKeyPBperChannel_[resolvedChannel - 1] + currentStripPBperChannel_[resolvedChannel - 1], -1.0f, 1.0f);
            float protocolValue = totalPB * 0.5f + 0.5f;
            sink.pushEvent(PerformanceEvent::pitchBend(resolvedChannel, -1, protocolValue, false, eventTime, zoneIndex));
        }
    } else if (midiValue.valueType == MidiValueType::ChannelAftertouch) {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        sink.pushEvent(PerformanceEvent::channelPressure(resolvedChannel, useNativePerNote ? noteNo : -1, val, useNativePerNote, eventTime, zoneIndex));
    } else if (midiValue.valueType == MidiValueType::PolyAftertouch) {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        sink.pushEvent(PerformanceEvent::polyAftertouch(resolvedChannel, noteNo, val, eventTime, zoneIndex));
    } else if (midiValue.valueType == MidiValueType::CC) {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        sink.pushEvent(PerformanceEvent::controllerChange(resolvedChannel, useNativePerNote ? noteNo : -1, midiValue.ccNo, val, useNativePerNote, eventTime, zoneIndex));
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
                    if (target == ExpressionCurveTarget::Yaw) gain = yawSensitivity_[deviceIndex];
                    else if (target == ExpressionCurveTarget::Roll) gain = rollSensitivity_[deviceIndex];
                    else if (target == ExpressionCurveTarget::Pressure) gain = pressureSensitivity_[deviceIndex];
                    
                    bool isBipolar = (target == ExpressionCurveTarget::Yaw || target == ExpressionCurveTarget::Roll);
                    float normalized = isBipolar ? (std::clamp(val * gain, -1.0f, 1.0f)) : (std::clamp(val * gain, 0.0f, 1.0f));
                    if (target == ExpressionCurveTarget::Roll)
                        normalized = applyRollPreCurve(normalized);
                    
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
        val *= breathSensitivity_[deviceIndex];
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

void MidiService::addStripValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, PerformanceEventSink& sink, bool isBipolar, int eventTime, MidiVoiceRouter* voiceRouter, int zoneIndex) {
    if (midiValue.valueType == MidiValueType::Off) return;
    
    int resolvedChannel = channel;
    if (resolvedChannel > 16 && voiceRouter) {
        resolvedChannel = voiceRouter->findMidiChannelForNewNote(static_cast<MidiChannelType>(channel), -1);
    }

    if (resolvedChannel < 1 || resolvedChannel > 16) return;

    int deviceIndex = static_cast<int>(deviceType) - 1;
    float gain = isBipolar ? 1.7f : 1.0f;
    if (deviceIndex >= 0 && deviceIndex < 3) {
        gain = stripSensitivity_[deviceIndex];
    }
    
    float normalized = isBipolar ? (std::clamp(ehValue * gain, -1.0f, 1.0f)) : (std::clamp(ehValue * gain, 0.0f, 1.0f));
    
    if (midiValue.valueType == MidiValueType::Pitchbend) {
        currentStripPBperChannel_[resolvedChannel - 1] = (isBipolar ? calculatePitchBendCurve(normalized) : normalized) * pbRange;
        float totalPB = std::clamp(currentKeyPBperChannel_[resolvedChannel - 1] + currentStripPBperChannel_[resolvedChannel - 1], -1.0f, 1.0f);
        float protocolValue = totalPB * 0.5f + 0.5f;
        
        juce::Logger::writeToLog("MidiService: Strip PB Message - channel=" + juce::String(resolvedChannel) + 
            ", rawEhValue=" + juce::String(ehValue) + ", normalized=" + juce::String(normalized) + 
            ", pbScaling=" + juce::String(pbRange) + 
            ", currentStripPB=" + juce::String(currentStripPBperChannel_[resolvedChannel - 1]) + 
            ", totalPB=" + juce::String(totalPB) + ", protocolValue=" + juce::String(protocolValue));

        sink.pushEvent(PerformanceEvent::pitchBend(resolvedChannel, -1, protocolValue, false, eventTime, zoneIndex));
    } else {
        float val = isBipolar ? (normalized * 0.5f + 0.5f) : normalized;
        if (midiValue.valueType == MidiValueType::ChannelAftertouch)
            sink.pushEvent(PerformanceEvent::channelPressure(resolvedChannel, -1, val, false, eventTime, zoneIndex));
        else if (midiValue.valueType == MidiValueType::CC)
            sink.pushEvent(PerformanceEvent::controllerChange(resolvedChannel, -1, midiValue.ccNo, val, false, eventTime, zoneIndex));
    }
}

float MidiService::applyStripCalibration(float eigenValue, bool invertStripDirection, float zeroThreshold, float sensitivity) {
    juce::ignoreUnused(sensitivity);

    const float orientedValue = invertStripDirection ? eigenValue : 1.0f - eigenValue;
    const float thresholdedValue = std::max(orientedValue - zeroThreshold, 0.0f);
    const float remainingRange = std::max(1.0f - zeroThreshold, std::numeric_limits<float>::epsilon());
    return std::clamp(thresholdedValue / remainingRange, 0.0f, 1.0f);
}

int MidiService::normalizeZoneIndex(int zoneIndex)
{
    return juce::jlimit(0, 2, zoneIndex);
}

int MidiService::zoneIndexFromKeyId(const LayoutWrapper::KeyId& keyId) const
{
    if (pluginState_ == nullptr)
        return 0;

    auto layoutKey = LayoutWrapper::getLayoutKey(keyId, pluginState_->state);
    return normalizeZoneIndex(static_cast<int>(layoutKey.zone) - 1);
}

void MidiService::createLayoutRPNs(juce::MidiBuffer& buffer) {
    auto* snapshot = activeSnapshot_.load(std::memory_order_acquire);
    auto transportSession = snapshot ? snapshot->transportSession : transportSession_;
    buffer.clear();
    if (transportSession)
        transportSession->setupTransport(buffer, mpeZone_);
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

bool MidiService::isNoteOnTransitionActive(const KeyState& state, ExpressionEmissionPolicy* expressionPolicy) const {
    const auto* policy = expressionPolicy != nullptr ? expressionPolicy : expressionPolicy_.get();
    const int transitionMs = policy != nullptr ? policy->getConfig().noteOnTransitionMilliseconds : 0;
    if (transitionMs <= 0 || state.noteOnTimestamp == 0 || state.lastTimestamp <= state.noteOnTimestamp)
        return false;

    const uint64_t transitionDurationUs = static_cast<uint64_t>(transitionMs) * 1000ULL;
    return (state.lastTimestamp - state.noteOnTimestamp) < transitionDurationUs;
}

float MidiService::calculateNoteOnVelocity(InstrumentType deviceType, KeyState* state) {
    if (state->ehPressureHistory.size() < PRESSURE_HISTORY_LENGTH) return 0.0f;
    
    auto it = state->ehPressureHistory.begin();
    int deviceIndex = static_cast<int>(deviceType) - 1;
    float gain = (deviceIndex >= 0 && deviceIndex < 3) ? pressureSensitivity_[deviceIndex] : 1.7f;

    float v1 = (it[1] + it[2]) / 2.0f;
    float v2 = (it[4] + it[5]) / 2.0f;
    float diff = (v2 - v1) * gain;
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
    int deviceIndex = static_cast<int>(deviceType) - 1;
    float gain = (deviceIndex >= 0 && deviceIndex < 3) ? pressureSensitivity_[deviceIndex] : 1.7f;

    const auto& history = state->ehPressureHistory;
    const auto historySize = history.size();
    const float releasePressure = history.back();
    const float recent1 = historySize >= 2U ? history[historySize - 2U] : releasePressure;
    const float recent2 = historySize >= 3U ? history[historySize - 3U] : recent1;
    const float recent3 = historySize >= 4U ? history[historySize - 4U] : recent2;

    const float heldPressure = recent1 * 0.5f + recent2 * 0.3f + recent3 * 0.2f;
    const float releaseDrop = std::max(heldPressure - releasePressure, 0.0f);
    const float relativeReleaseSpeed = releaseDrop / std::max(heldPressure, 0.01f);
    const float heldPressureFloor = std::clamp(heldPressure * gain * 6.0f, 0.0f, 1.0f);
    float norm = std::clamp(relativeReleaseSpeed * 0.85f + heldPressureFloor * 0.15f, 0.0f, 1.0f);
    norm = std::pow(norm, 0.65f);

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

void MidiService::processMessage (juce::universal_midi_packets::BytesOnGroup msg) {
    if (!initialized_) return;
    const juce::ScopedLock sl(umpOutputLock_);
    
    auto sendToOutput = [&](juce::universal_midi_packets::Output& output) {
        if (output.isAlive()) {
            juce::universal_midi_packets::Conversion::umpFrom7BitData(msg, [&output](const juce::universal_midi_packets::View& v) {
                using namespace juce::universal_midi_packets;
                output.send(Iterator(v.data(), v.size()), Iterator(v.data() + v.size(), 0));
            });
        }
    };

    if (isVirtualTarget_ && isMidi2Mode_) {
        if (msg.group < directUmpOutputs_.size())
            sendToOutput(directUmpOutputs_[msg.group]);
    } else {
        sendToOutput(umpOutput_);

        if (isFirstInstance_ && isMidi2Mode_)
            for (auto& directUmpOutput : directUmpOutputs_)
                sendToOutput(directUmpOutput);
    }
}

} // namespace ecm
