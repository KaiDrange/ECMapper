#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/SettingsWrapper.h"
#include "Core/Midi2Protocol.h"
#include <cmath>
#include <string_view>

namespace {

constexpr int kTransposeCcNumber = 22;
constexpr int kZoneEnableCcNumber = 23;
constexpr int kPresetParameterDefaultIndex = 0;

int transposeFromCc(int ccValue)
{
    ccValue = juce::jlimit(0, 127, ccValue);

    if (ccValue == 64)
        return 0;

    if (ccValue < 64)
        return juce::jlimit(-96, 0, juce::roundToInt(juce::jmap(static_cast<float>(ccValue), 0.0f, 63.0f, -96.0f, -1.0f)));

    return juce::jlimit(0, 96, juce::roundToInt(juce::jmap(static_cast<float>(ccValue), 65.0f, 127.0f, 1.0f, 96.0f)));
}

bool enableFromCc(const int ccValue)
{
    return juce::jlimit(0, 127, ccValue) >= 64;
}

struct DeviceKeyCounts
{
    int normal = 0;
    int perc = 0;
    int buttons = 0;
};

DeviceKeyCounts getDeviceKeyCounts(const ecm::InstrumentType deviceType)
{
    switch (deviceType) {
        case ecm::InstrumentType::Alpha: return { 120, 12, 0 };
        case ecm::InstrumentType::Tau:   return { 72, 12, 8 };
        case ecm::InstrumentType::Pico:  return { 18, 0, 4 };
        default:                         return {};
    }
}

bool isPresetMpeProperty(const juce::Identifier& property)
{
    return property == ecm::SettingsWrapper::id_lowerMPEVoiceCount
        || property == ecm::SettingsWrapper::id_upperMPEVoiceCount
        || property == ecm::SettingsWrapper::id_lowerMPEPB
        || property == ecm::SettingsWrapper::id_upperMPEPB
        || property == ecm::SettingsWrapper::id_midi2Mode;
}

void pruneGlobalSettingsForPreset(juce::ValueTree& globalSettings)
{
    if (!globalSettings.isValid())
        return;

    for (int i = globalSettings.getNumChildren(); --i >= 0;)
        globalSettings.removeChild(i, nullptr);

    for (int i = globalSettings.getNumProperties(); --i >= 0;) {
        auto property = globalSettings.getPropertyName(i);
        if (!isPresetMpeProperty(property))
            globalSettings.removeProperty(property, nullptr);
    }
}

void applyGlobalSettingsPreset(juce::ValueTree& liveRoot, const juce::ValueTree& presetGlobalSettings)
{
    auto liveGlobalSettings = liveRoot.getOrCreateChildWithName(ecm::SettingsWrapper::id_globalSettings, nullptr);

    auto copyProperty = [&](const juce::Identifier& property)
    {
        if (presetGlobalSettings.hasProperty(property))
            liveGlobalSettings.setProperty(property, presetGlobalSettings.getProperty(property), nullptr);
        else
            liveGlobalSettings.removeProperty(property, nullptr);
    };

    copyProperty(ecm::SettingsWrapper::id_lowerMPEVoiceCount);
    copyProperty(ecm::SettingsWrapper::id_upperMPEVoiceCount);
    copyProperty(ecm::SettingsWrapper::id_lowerMPEPB);
    copyProperty(ecm::SettingsWrapper::id_upperMPEPB);
    copyProperty(ecm::SettingsWrapper::id_midi2Mode);
}

void materializeLayoutKeysForDevice(const ecm::InstrumentType deviceType, juce::ValueTree& rootState)
{
    auto counts = getDeviceKeyCounts(deviceType);

    auto materializeKey = [&](const ecm::LayoutWrapper::KeyId& keyId)
    {
        auto key = ecm::LayoutWrapper::getLayoutKey(keyId, rootState);
        ecm::LayoutWrapper::setLayoutKey(key, rootState);
    };

    for (int keyNo = 0; keyNo < counts.normal; ++keyNo)
        materializeKey({ 0, keyNo, deviceType });

    switch (deviceType) {
        case ecm::InstrumentType::Alpha:
            for (int keyNo = 0; keyNo < counts.perc; ++keyNo)
                materializeKey({ 1, keyNo, deviceType });
            break;
        case ecm::InstrumentType::Tau:
            for (int keyNo = 72; keyNo < 72 + counts.perc; ++keyNo)
                materializeKey({ 0, keyNo, deviceType });
            for (int keyNo = 5; keyNo < 5 + counts.buttons; ++keyNo)
                materializeKey({ 1, keyNo, deviceType });
            break;
        case ecm::InstrumentType::Pico:
            for (int keyNo = 0; keyNo < counts.buttons; ++keyNo)
                materializeKey({ 1, keyNo, deviceType });
            break;
        default:
            break;
    }
}

void materializeZoneState(const ecm::InstrumentType deviceType, const ecm::Zone zone, juce::ValueTree& rootState)
{
    ecm::ZoneWrapper::setEnabled(deviceType, zone, ecm::ZoneWrapper::getEnabled(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setTranspose(deviceType, zone, ecm::ZoneWrapper::getTranspose(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setKeyPitchbend(deviceType, zone, ecm::ZoneWrapper::getKeyPitchbend(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setChannelMaxPitchbend(deviceType, zone, ecm::ZoneWrapper::getChannelMaxPitchbend(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setMidiChannelType(deviceType, zone, ecm::ZoneWrapper::getMidiChannelType(deviceType, zone, rootState), rootState);

    auto setMidiValue = [&](const juce::Identifier& childId, const ecm::ZoneWrapper::MidiValue defaultValue)
    {
        ecm::ZoneWrapper::setMidiValue(deviceType, zone, childId, ecm::ZoneWrapper::getMidiValue(deviceType, zone, childId, defaultValue, rootState), rootState);
    };

    setMidiValue(ecm::ZoneWrapper::id_pressure, ecm::ZoneWrapper::default_pressure);
    setMidiValue(ecm::ZoneWrapper::id_roll, ecm::ZoneWrapper::default_roll);
    setMidiValue(ecm::ZoneWrapper::id_yaw, ecm::ZoneWrapper::default_yaw);
    setMidiValue(ecm::ZoneWrapper::id_strip1Rel, ecm::ZoneWrapper::default_strip1Rel);
    setMidiValue(ecm::ZoneWrapper::id_strip1Abs, ecm::ZoneWrapper::default_strip1Abs);
    setMidiValue(ecm::ZoneWrapper::id_strip2Rel, ecm::ZoneWrapper::default_strip2Rel);
    setMidiValue(ecm::ZoneWrapper::id_strip2Abs, ecm::ZoneWrapper::default_strip2Abs);
    setMidiValue(ecm::ZoneWrapper::id_breath, ecm::ZoneWrapper::default_breath);
}

void materializeCurveState(ecm::InstrumentType deviceType, juce::ValueTree& rootState)
{
    for (int i = 0; i < 6; ++i) {
        auto target = static_cast<ecm::ExpressionCurveTarget>(i);
        auto curve = ecm::ExpressionCurveWrapper::getCurve(deviceType, target, rootState);
        ecm::ExpressionCurveWrapper::setCurve(deviceType, target, curve, rootState);
    }
}

void materializePresetState(juce::ValueTree& rootState)
{
    for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
        const auto deviceType = static_cast<ecm::InstrumentType>(device);
        materializeLayoutKeysForDevice(deviceType, rootState);
        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone)
            materializeZoneState(deviceType, static_cast<ecm::Zone>(zone), rootState);
        materializeCurveState(deviceType, rootState);
    }

    ecm::SettingsWrapper::setLowerMPEVoiceCount(ecm::SettingsWrapper::getLowerMPEVoiceCount(rootState), rootState);
    ecm::SettingsWrapper::setUpperMPEVoiceCount(ecm::SettingsWrapper::getUpperMPEVoiceCount(rootState), rootState);
    ecm::SettingsWrapper::setLowerMPEPB(ecm::SettingsWrapper::getLowerMPEPB(rootState), rootState);
    ecm::SettingsWrapper::setUpperMPEPB(ecm::SettingsWrapper::getUpperMPEPB(rootState), rootState);
    ecm::SettingsWrapper::setMidi2Mode(ecm::SettingsWrapper::getMidi2Mode(rootState), rootState);
}

void mergeTreeIntoLive(juce::ValueTree& liveTree, const juce::ValueTree& snapshotTree)
{
    if (!liveTree.isValid() || !snapshotTree.isValid())
        return;

    jassert(liveTree.getType() == snapshotTree.getType());

    for (int i = 0; i < snapshotTree.getNumProperties(); ++i) {
        auto property = snapshotTree.getPropertyName(i);
        liveTree.setProperty(property, snapshotTree.getProperty(property), nullptr);
    }

    for (int i = 0; i < snapshotTree.getNumChildren(); ++i) {
        auto child = snapshotTree.getChild(i);
        auto liveChild = liveTree.getChildWithName(child.getType());
        if (!liveChild.isValid()) {
            liveTree.addChild(child.createCopy(), -1, nullptr);
            liveChild = liveTree.getChildWithName(child.getType());
        }

        mergeTreeIntoLive(liveChild, child);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createTransposeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray presetChoices;
    for (int slot = 1; slot <= ECMapperAudioProcessor::numPresetSlots; ++slot)
        presetChoices.add(juce::String(slot));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ECMapperAudioProcessor::presetSlotParameterId, 1 },
        "Preset Slot",
        presetChoices,
        kPresetParameterDefaultIndex));

    for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            auto paramId = ecm::ZoneWrapper::getTransposeParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone));
            auto paramName = juce::String::formatted("Transpose %d-%d", device, zone);
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { paramId, 1 }, paramName, -96, 96, 0));

            auto enabledParamId = ecm::ZoneWrapper::getEnabledParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone));
            auto enabledParamName = juce::String::formatted("Enable %d-%d", device, zone);
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { enabledParamId, 1 }, enabledParamName, true));
        }
    }

    return layout;
}

}

ECMapperAudioProcessor::ECMapperAudioProcessor() :
    AudioProcessor(BusesProperties()
               .withInput("Input", juce::AudioChannelSet::stereo(), true)
               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "ECMapperState", createParameterLayout()),
    configLookups {
        ecm::ConfigLookup(ecm::InstrumentType::Alpha, state, presetStateLock_),
        ecm::ConfigLookup(ecm::InstrumentType::Tau, state, presetStateLock_),
        ecm::ConfigLookup(ecm::InstrumentType::Pico, state, presetStateLock_)
    },
    hardwareService(hardwareToMapperQueue, mapperToHardwareQueue),
    midiService(configLookups, presetStateLock_),
    oscBridge(hardwareService, hardwareToMapperQueue, mapperToHardwareQueue, outgoingOSCQueue, logger) {
    
    hardwareService.addListener(this);
    hardwareService.setOSCBroadcastQueue(&outgoingOSCQueue);
    midiService.setOSCBroadcastQueue(&outgoingOSCQueue);
    midiService.setLocalHardwareQueue(&mapperToHardwareQueue);

    presetSlotParameter_ = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(presetSlotParameterId));
    jassert(presetSlotParameter_ != nullptr);
    currentPresetSlot_.store(1);
    currentPresetName_ = "Init";
    lastPresetParameterIndex_.store(presetSlotParameter_->getIndex());
    registerZoneParameterListeners();
    
    layoutChangeHandler = std::make_unique<ecm::LayoutChangeHandler>(
        mapperToHardwareQueue,
        state.state,
        configLookups,
        presetStateLock_,
        [this]() { return presetBatchInProgress_; },
        [this](ecm::InstrumentType deviceType, ecm::Zone zone) { 
            if (deviceType != ecm::InstrumentType::None)
                midiService.queueTransposeChangeFlush(deviceType, zone);
            requestRuntimeConfigRefresh();
        });
    state.state.addListener(layoutChangeHandler.get());
}

ECMapperAudioProcessor::~ECMapperAudioProcessor() {
    unregisterZoneParameterListeners();
    hardwareService.removeListener(this);
    state.state.removeListener(layoutChangeHandler.get());
}

void ECMapperAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    logger.log("prepareToPlay() called.");
    
    updateGlobalSettings();
    midiService.start(state, &hardwareService);
    refreshZoneRuntimeStateFromParameters();
    hardwareService.startService(&state.state);
    oscBridge.setSenderEnabled(true);
    oscBridge.setReceiverEnabled(true);
    
    lastBlockEndUs = 0.0;
    localClockOffset = 0.0;
    remoteClockOffsets.clear();
    
    logger.log("prepareToPlay() finished.");
}

void ECMapperAudioProcessor::releaseResources() {
    logger.log("releaseResources() called.");
    midiService.stop();
    hardwareService.stopService();
    oscBridge.setSenderEnabled(false);
    oscBridge.setReceiverEnabled(false);
    logger.log("releaseResources() finished.");
}

bool ECMapperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return true;
}

void ECMapperAudioProcessor::processBlock(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiMessages) {
    if (juce::JUCEApplicationBase::isStandaloneApp())
        audioBuffer.clear();

    juce::MidiBuffer* targetBuffer = &midiMessages;
    juce::MidiBuffer tempBuffer;
    bool useDirect = ecm::SettingsWrapper::getMidi2Mode(state.state) && juce::JUCEApplicationBase::isStandaloneApp();
    
    if (useDirect)
        targetBuffer = &tempBuffer;

    int slotToLoad = -1;
    collectPresetSlotLoadRequests(midiMessages, slotToLoad);
    const auto timing = calculateBlockTiming(audioBuffer);
    if (applyZoneControlMessages(midiMessages))
        requestRuntimeConfigRefresh();
    prepareMidiMessagesForBlock(*targetBuffer);
    processHardwareMessagesForBlock(timing, *targetBuffer, slotToLoad);
    dispatchPresetSlotLoad(slotToLoad);
    midiService.reduceBreath(*targetBuffer, timing.numSamples - 1);
    
    if (useDirect) {
        midiService.drainDirectUMPs(tempBuffer);
        midiMessages.clear();
    }
    
    midiService.finishedBlock();
}

ECMapperAudioProcessor::BlockTiming ECMapperAudioProcessor::calculateBlockTiming(const juce::AudioBuffer<float>& audioBuffer)
{
    BlockTiming timing;
    timing.numSamples = audioBuffer.getNumSamples();
    timing.sampleRate = getSampleRate();
    timing.blockDurationUs = 1000000.0 * timing.numSamples / timing.sampleRate;
    timing.nowUs = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()) * 1000000.0;

    if (lastBlockEndUs == 0.0 || std::abs(timing.nowUs - lastBlockEndUs) > 500000.0) {
        lastBlockEndUs = timing.nowUs - timing.blockDurationUs;
        localClockOffset = 0.0;
        remoteClockOffsets.clear();
    }

    timing.blockStartUs = lastBlockEndUs;
    lastBlockEndUs = timing.blockStartUs + timing.blockDurationUs;
    return timing;
}

void ECMapperAudioProcessor::prepareMidiMessagesForBlock(juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn() || msg.isNoteOff())
            queueKeyboardSelectionMessage(msg);
    }

    midiMessages.clear();

    if (!layoutChangeHandler->layoutMidiRPNSent) {
        midiService.createLayoutRPNs(midiMessages);
        layoutChangeHandler->layoutMidiRPNSent = true;
    }

    midiService.drainPendingMidiMessages(midiMessages, 0);
}

void ECMapperAudioProcessor::processHardwareMessagesForBlock(const BlockTiming& timing, juce::MidiBuffer& midiMessages, int& slotToLoad)
{
    ecm::osc::Message msg;

    while (hardwareToMapperQueue.read(msg)) {
        handleHardwareMessage(msg, timing, midiMessages, slotToLoad);
    }
}

void ECMapperAudioProcessor::handleHardwareMessage(const ecm::osc::Message& msg, const BlockTiming& timing, juce::MidiBuffer& midiMessages, int& slotToLoad)
{
    if (msg.type == ecm::osc::MessageType::Device) {
        layoutChangeHandler->sendLEDMsgForAllKeys(msg.device);
        return;
    }

    int sampleOffset = 0;
    if (msg.timestamp > 0) {
        auto localMsgTime = static_cast<double>(msg.timestamp);

        if (msg.isRemote) {
            const std::string_view devId { msg.devId };
            auto it = remoteClockOffsets.find(devId);
            if (it == remoteClockOffsets.end()) {
                it = remoteClockOffsets.emplace(std::string(devId), timing.nowUs - static_cast<double>(msg.timestamp)).first;
            }
            localMsgTime += it->second;
        } else {
            if (localClockOffset == 0.0)
                localClockOffset = timing.nowUs - static_cast<double>(msg.timestamp);
            localMsgTime += localClockOffset;
        }

        const double offsetUs = localMsgTime - timing.blockStartUs;
        sampleOffset = static_cast<int>(offsetUs * timing.sampleRate / 1000000.0);
        sampleOffset = std::clamp(sampleOffset, 0, timing.numSamples - 1);
    }

    ecm::osc::Message outgoingMsg;
    outgoingMsg.type = ecm::osc::MessageType::Undefined;
    int presetSlotRequest = -1;
    midiService.processMessage(msg, outgoingMsg, midiMessages, sampleOffset, &presetSlotRequest);

    if (outgoingMsg.type == ecm::osc::MessageType::LED) {
        if (hardwareService.getDeviceMode(msg.devId) == ecm::DeviceMode::Local)
            mapperToHardwareQueue.add(outgoingMsg);
    } else {
        queuePresetSlotLoad(presetSlotRequest, slotToLoad);
    }
}

void ECMapperAudioProcessor::dispatchPresetSlotLoad(const int slotToLoad)
{
    if (slotToLoad == -1)
        return;

    slotToLoadAsync_ = slotToLoad;
    triggerAsyncUpdate();
}

void ECMapperAudioProcessor::publishRuntimeConfigSnapshot()
{
    logger.log("publishRuntimeConfigSnapshot: Updating snapshot with protocol " + juce::String(midiService.getProtocol() ? (std::dynamic_pointer_cast<ecm::Midi2Protocol>(midiService.getProtocol()) ? "MIDI 2.0" : "MIDI 1.0") : "None"));
    midiService.setRuntimeConfigSnapshot(std::make_unique<ecm::MidiService::RuntimeConfigSnapshot>(configLookups, midiService.getProtocol()));
}

bool ECMapperAudioProcessor::applyZoneControlMessages(const juce::MidiBuffer& midiMessages) const
{
    bool changed = false;

    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (!msg.isController())
            continue;

        const auto channel = msg.getChannel();
        if (channel < 1 || channel > 4)
            continue;

        if (msg.getControllerNumber() == kTransposeCcNumber) {
            for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
                for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
                    if (channel != 4 && zone != channel)
                        continue;

                    const auto transposeValue = transposeFromCc(msg.getControllerValue());
                    const auto deviceType = static_cast<ecm::InstrumentType>(device);
                    const auto zoneType = static_cast<ecm::Zone>(zone);
                    const auto paramId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
                    if (auto* raw = state.getRawParameterValue(paramId)) {
                        if (static_cast<int>(std::lround(raw->load())) != transposeValue) {
                            if (auto* param = dynamic_cast<juce::AudioParameterInt*>(state.getParameter(paramId))) {
                                const auto normalised = param->getNormalisableRange().convertTo0to1(static_cast<float>(transposeValue));
                                param->setValueNotifyingHost(normalised);
                            }
                            changed = true;
                        }
                    }
                }
            }
            continue;
        }

        if (msg.getControllerNumber() != kZoneEnableCcNumber)
            continue;

        const bool enabled = enableFromCc(msg.getControllerValue());

        for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
            for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
                if (channel != 4 && zone != channel)
                    continue;

                const auto deviceType = static_cast<ecm::InstrumentType>(device);
                const auto zoneType = static_cast<ecm::Zone>(zone);
                const auto paramId = ecm::ZoneWrapper::getEnabledParameterID(deviceType, zoneType);
                if (auto* raw = state.getRawParameterValue(paramId)) {
                    const auto value = enabled ? 1.0f : 0.0f;
                    if ((raw->load() > 0.5f) != enabled) {
                        if (auto* param = dynamic_cast<juce::AudioParameterBool*>(state.getParameter(paramId)))
                            param->setValueNotifyingHost(value);
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

void ECMapperAudioProcessor::requestRuntimeConfigRefresh()
{
    if (!runtimeConfigRefreshRequested_.exchange(true))
        triggerAsyncUpdate();
}

void ECMapperAudioProcessor::refreshZoneRuntimeStateFromParameters()
{
    if (!midiService.isInitialized())
        return;

    const juce::ScopedLock stateGuard(presetStateLock_);
    const juce::ScopedValueSetter<bool> batchGuard(presetBatchInProgress_, true);

    bool deviceNeedsUpdate[3] = { false, false, false };

    if (!transposeCacheInitialised_) {
        for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
            for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
                const auto idx = transposeIndex(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone));
                if (const auto* raw = state.getRawParameterValue(ecm::ZoneWrapper::getTransposeParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone))))
                    transposeCache_[idx] = static_cast<int>(std::lround(raw->load()));
            }
        }
        transposeCacheInitialised_ = true;
    }

    if (!enableCacheInitialised_) {
        for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
            for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
                const auto idx = transposeIndex(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone));
                if (const auto* raw = state.getRawParameterValue(ecm::ZoneWrapper::getEnabledParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone))))
                    enableCache_[idx] = raw->load() > 0.5f ? 1 : 0;
            }
        }
        enableCacheInitialised_ = true;
    }

    for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            const auto deviceType = static_cast<ecm::InstrumentType>(device);
            const auto zoneType = static_cast<ecm::Zone>(zone);
            const auto idx = transposeIndex(deviceType, zoneType);

            auto transposeId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
            int currentTranspose = transposeCache_[idx];
            if (const auto* raw = state.getRawParameterValue(transposeId))
                currentTranspose = static_cast<int>(std::lround(raw->load()));

            if (currentTranspose != transposeCache_[idx]) {
                midiService.queueTransposeChangeFlush(deviceType, zoneType);
                transposeCache_[idx] = currentTranspose;
                deviceNeedsUpdate[device - 1] = true;
            }

            int currentEnabled = enableCache_[idx];
            if (const auto* raw = state.getRawParameterValue(ecm::ZoneWrapper::getEnabledParameterID(deviceType, zoneType)))
                currentEnabled = raw->load() > 0.5f ? 1 : 0;

            if (currentEnabled != enableCache_[idx]) {
                const auto enabled = currentEnabled != 0;
                if (ecm::ZoneWrapper::getEnabled(deviceType, zoneType, state.state) != enabled)
                    ecm::ZoneWrapper::setEnabled(deviceType, zoneType, enabled, state.state);
                enableCache_[idx] = currentEnabled;
                deviceNeedsUpdate[device - 1] = true;
            }
        }
    }

    for (int device = 0; device < 3; ++device) {
        if (!deviceNeedsUpdate[device])
            continue;

        configLookups[device].updateAll();
        layoutChangeHandler->sendLEDMsgForAllKeys(static_cast<ecm::InstrumentType>(device + 1));
    }

    publishRuntimeConfigSnapshot();
}

void ECMapperAudioProcessor::collectPresetSlotLoadRequests(const juce::MidiBuffer& midiMessages, int& slotToLoad)
{
    if (presetSlotParameter_ != nullptr) {
        const auto selectedIndex = presetSlotParameter_->getIndex();
        if (ignorePresetParameterUpdate_.exchange(false)) {
            lastPresetParameterIndex_.store(selectedIndex);
        } else if (selectedIndex != lastPresetParameterIndex_.load()) {
            queuePresetSlotLoad(selectedIndex + 1, slotToLoad);
            lastPresetParameterIndex_.store(selectedIndex);
        }
    }

    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isProgramChange())
            queuePresetSlotLoad(msg.getProgramChangeNumber() + 1, slotToLoad);
    }
}

void ECMapperAudioProcessor::queuePresetSlotLoad(const int slot, int& slotToLoad)
{
    if (slot >= 1 && slot <= numPresetSlots)
        slotToLoad = slot;
}

juce::AudioProcessorEditor* ECMapperAudioProcessor::createEditor() {
    return new ECMapperAudioProcessorEditor(*this);
}

void ECMapperAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const juce::ScopedLock stateGuard(presetStateLock_);
    juce::ValueTree bundle("ECMapperStateBundle");
    bundle.addChild(state.copyState(), -1, nullptr);
    bundle.addChild(presetBankState_.createCopy(), -1, nullptr);
    const std::unique_ptr xml(bundle.createXml());
    copyXmlToBinary(*xml, destData);
}

void ECMapperAudioProcessor::setStateInformation(const void* data, const int sizeInBytes) {
    {
        const juce::ScopedLock stateGuard(presetStateLock_);
        const std::unique_ptr xmlState(getXmlFromBinary(data, sizeInBytes));
        if (xmlState == nullptr)
            return;

        const auto tree = juce::ValueTree::fromXml(*xmlState);
        if (!tree.isValid())
            return;

        if (tree.hasType("ECMapperStateBundle")) {
            const juce::ScopedValueSetter<bool> batchGuard(presetBatchInProgress_, true);
            const auto liveState = tree.getChildWithName(state.state.getType());
            if (liveState.isValid())
                mergeTreeIntoLive(state.state, liveState);

            const auto bankState = tree.getChildWithName(presetBankState_.getType());
            if (bankState.isValid())
                presetBankState_ = bankState;
        } else if (tree.hasType(state.state.getType())) {
            const juce::ScopedValueSetter batchGuard(presetBatchInProgress_, true);
            mergeTreeIntoLive(state.state, tree);
        }

        presetSlotParameter_ = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(presetSlotParameterId));
        const auto lastPresetIndex = presetSlotParameter_ != nullptr ? presetSlotParameter_->getIndex() : 0;
        lastPresetParameterIndex_.store(lastPresetIndex);
        currentPresetSlot_.store(juce::jlimit(1, numPresetSlots, lastPresetIndex + 1));
        {
            const auto presetNode = getPresetNode(currentPresetSlot_.load());
            currentPresetName_ = presetNode.isValid() ? presetNode.getProperty("name", juce::String()).toString()
                                                      : (currentPresetSlot_.load() == 1 ? juce::String("Init") : juce::String("Empty"));
        }
        ensureInitPresetExists();
        ignorePresetParameterUpdate_.store(true);
        {
            const juce::ScopedValueSetter batchGuard(presetBatchInProgress_, true);
            refreshDerivedStateAfterPresetChange();
        }
    }
    updateGlobalSettings();
}

void ECMapperAudioProcessor::updateGlobalSettings() {
    ecm::AppRole role;
    juce::String clientIP;
    int clientPort;

    {
        const juce::ScopedLock stateGuard(presetStateLock_);
        role = ecm::SettingsWrapper::getAppRole(state.state);

        if (!ecm::HardwareService::supportsLocalHardware()) {
            role = ecm::AppRole::Client;
        } else if (role == ecm::AppRole::Host && hardwareService.getAppRole() != ecm::AppRole::Host && ecm::OSCBridge::isPortOccupied(12121)) {
            logger.log("Host detected on network (port 12121 busy). Auto-switching to Client mode.");
            role = ecm::AppRole::Client;
        }

        if (ecm::SettingsWrapper::getAppRole(state.state) != role)
            ecm::SettingsWrapper::setAppRole(role, state.state);

        clientIP = ecm::SettingsWrapper::getClientListenIP(state.state);
        clientPort = ecm::SettingsWrapper::getClientListenPort(state.state);
        
        const bool midi2 = ecm::SettingsWrapper::getMidi2Mode(state.state);
        logger.log("updateGlobalSettings: MIDI 2.0 Mode is " + juce::String(midi2 ? "Enabled" : "Disabled"));
    }

    hardwareService.setAppRole(role);
    hardwareService.setClientListenSettings(clientIP, clientPort);
}

juce::AudioProcessorValueTreeState::ParameterLayout ECMapperAudioProcessor::createParameterLayout()
{
    return createTransposeParameters();
}

juce::String ECMapperAudioProcessor::getCurrentPresetName() const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    return currentPresetName_;
}

juce::String ECMapperAudioProcessor::getCurrentPresetDisplayName() const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    return getPresetSlotDisplayName(currentPresetSlot_.load());
}

juce::String ECMapperAudioProcessor::getPresetSlotDisplayName(const int slot) const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (slot < 1 || slot > numPresetSlots)
        return {};

    const auto label = juce::String(slot) + ": ";
    const auto preset = getPresetNode(slot);
    auto presetName = preset.isValid() ? preset.getProperty("name", juce::String()).toString() : juce::String();

    if (slot == 1)
    {
        if (presetName.isEmpty())
            presetName = "Init";

        return label + presetName;
    }

    if (preset.isValid()) {
        if (presetName.isEmpty())
            presetName = "Preset";
        return label + presetName;
    }

    return label + "Empty";
}

bool ECMapperAudioProcessor::hasPresetSlot(const int slot) const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (slot < 1 || slot > numPresetSlots)
        return false;

    const auto slotValue = slot;
    for (int i = 0; i < presetBankState_.getNumChildren(); ++i) {
        auto preset = presetBankState_.getChild(i);
        if (static_cast<int>(preset.getProperty("slot", 0)) == slotValue)
            return true;
    }

    return false;
}

juce::ValueTree ECMapperAudioProcessor::getPresetNode(const int slot) const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (slot < 1 || slot > numPresetSlots)
        return {};

    for (int i = 0; i < presetBankState_.getNumChildren(); ++i) {
        auto preset = presetBankState_.getChild(i);
        if (static_cast<int>(preset.getProperty("slot", 0)) == slot)
            return preset;
    }

    return {};
}

juce::ValueTree ECMapperAudioProcessor::getPresetSnapshot(const int slot) const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    auto preset = getPresetNode(slot);
    if (!preset.isValid())
        return {};

    auto snapshot = preset.getChildWithName(state.state.getType());
    if (snapshot.isValid())
        return snapshot;

    return {};
}

void ECMapperAudioProcessor::setCurrentPresetSelection(const int slot, const juce::String& name)
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    currentPresetSlot_.store(juce::jlimit(1, numPresetSlots, slot));
    currentPresetName_ = name;

    if (presetSlotParameter_ != nullptr) {
        const auto index = currentPresetSlot_.load() - 1;
        if (presetSlotParameter_->getIndex() != index) {
            ignorePresetParameterUpdate_.store(true);
            presetSlotParameter_->setValueNotifyingHost(presetSlotParameter_->convertTo0to1(static_cast<float>(index)));
            ignorePresetParameterUpdate_.store(false);
        }
        lastPresetParameterIndex_.store(index);
    }
}

void ECMapperAudioProcessor::applyPresetState(const juce::ValueTree& snapshot)
{
    if (!snapshot.isValid())
        return;

    const juce::ScopedLock stateGuard(presetStateLock_);
    {
        const juce::ScopedValueSetter batchGuard(presetBatchInProgress_, true);
        auto& liveState = state.state;
        mergeTreeIntoLive(liveState, snapshot);

        auto snapshotGlobalSettings = snapshot.getChildWithName(ecm::SettingsWrapper::id_globalSettings);
        if (snapshotGlobalSettings.isValid()) {
            applyGlobalSettingsPreset(liveState, snapshotGlobalSettings);
        } else {
            applyGlobalSettingsPreset(liveState, juce::ValueTree());
        }

        for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
            for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
                const auto deviceType = static_cast<ecm::InstrumentType>(device);
                const auto zoneType = static_cast<ecm::Zone>(zone);

                auto transposeId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
                if (auto* param = dynamic_cast<juce::AudioParameterInt*>(state.getParameter(transposeId))) {
                    const auto transposeValue = ecm::ZoneWrapper::getTranspose(deviceType, zoneType, liveState);
                    if (const auto* raw = state.getRawParameterValue(transposeId)) {
                        auto currentValue = static_cast<int>(std::lround(raw->load()));
                        if (currentValue != transposeValue)
                            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(static_cast<float>(transposeValue)));
                    }
                }

                auto enabledId = ecm::ZoneWrapper::getEnabledParameterID(deviceType, zoneType);
                if (auto* param = dynamic_cast<juce::AudioParameterBool*>(state.getParameter(enabledId))) {
                    const auto enabledValue = ecm::ZoneWrapper::getEnabled(deviceType, zoneType, liveState);
                    if (const auto* raw = state.getRawParameterValue(enabledId)) {
                        const auto currentValue = raw->load() > 0.5f;
                        if (currentValue != enabledValue)
                            param->setValueNotifyingHost(enabledValue ? 1.0f : 0.0f);
                    }
                }
            }
        }

        refreshDerivedStateAfterPresetChange();
    }
}

void ECMapperAudioProcessor::refreshDerivedStateAfterPresetChange()
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    auto& liveState = state.state;

    for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
        const auto deviceType = static_cast<ecm::InstrumentType>(device);
        const auto deviceIndex = device - 1;
        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            const auto zoneType = static_cast<ecm::Zone>(zone);
            const auto idx = transposeIndex(deviceType, zoneType);

            const int transposeValue = ecm::ZoneWrapper::getTranspose(deviceType, zoneType, liveState);
            if (transposeCacheInitialised_ && transposeCache_[idx] != transposeValue) {
                midiService.queueTransposeChangeFlush(deviceType, zoneType);
            }
            transposeCache_[idx] = transposeValue;

            const int enabledValue = ecm::ZoneWrapper::getEnabled(deviceType, zoneType, liveState) ? 1 : 0;
            enableCache_[idx] = enabledValue;
        }

        if (!midiService.isInitialized())
            continue;

        configLookups[deviceIndex].updateAll();
        layoutChangeHandler->sendLEDMsgForAllKeys(deviceType);
    }

    transposeCacheInitialised_ = true;
    enableCacheInitialised_ = true;
    publishRuntimeConfigSnapshot();
}

juce::ValueTree ECMapperAudioProcessor::makeComparableState(juce::ValueTree stateTree)
{
    if (stateTree.isValid())
        stateTree.removeProperty(presetSlotParameterId, nullptr);

    materializePresetState(stateTree);

    auto globalSettings = stateTree.getChildWithName(ecm::SettingsWrapper::id_globalSettings);
    if (globalSettings.isValid())
        pruneGlobalSettingsForPreset(globalSettings);

    return stateTree;
}

void ECMapperAudioProcessor::handleAsyncUpdate()
{
    const int slot = slotToLoadAsync_.exchange(-1);
    if (slot != -1)
        loadPresetSlot(slot);

    if (runtimeConfigRefreshRequested_.exchange(false))
        refreshZoneRuntimeStateFromParameters();
}

void ECMapperAudioProcessor::loadStandalonePresetBank()
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        return;

    const auto file = getStandalonePresetBankFile();
    if (!file.existsAsFile()) {
        ensureInitPresetExists();
        saveStandalonePresetBank();
        return;
    }

    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return;

    const auto bundle = juce::ValueTree::fromXml(*xml);
    if (!bundle.isValid())
        return;

    if (bundle.hasType("ECMapperPresetBank")) {
        presetBankState_ = bundle;
    } else if (bundle.hasType("ECMapperStateBundle")) {
        const auto bank = bundle.getChildWithName(presetBankState_.getType());
        if (bank.isValid())
            presetBankState_ = bank;
    }

    ensureInitPresetExists();
}

void ECMapperAudioProcessor::saveStandalonePresetBank() const
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        return;

    const auto file = getStandalonePresetBankFile();
    if (!file.getParentDirectory().exists())
        // ReSharper disable once CppExpressionWithoutSideEffects
        file.getParentDirectory().createDirectory();

    const juce::ValueTree bankCopy = presetBankState_.createCopy();
    const std::unique_ptr xml(bankCopy.createXml());
    if (xml != nullptr)
        // ReSharper disable once CppExpressionWithoutSideEffects
        xml->writeTo(file);
}

juce::File ECMapperAudioProcessor::getStandalonePresetBankFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ECMapper")
        .getChildFile("preset_bank.xml");
}

bool ECMapperAudioProcessor::savePresetSlot(const int slot, const juce::String& name)
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (slot < 1 || slot > numPresetSlots)
        return false;

    const auto snapshot = makeComparableState(state.state.createCopy());

    auto preset = getPresetNode(slot);
    if (preset.isValid()) {
        presetBankState_.removeChild(preset, nullptr);
    } else {
        preset = juce::ValueTree("ECMapperPreset");
    }

    preset.setProperty("slot", slot, nullptr);
    preset.setProperty("name", name, nullptr);
    preset.removeAllChildren(nullptr);
    preset.addChild(snapshot, -1, nullptr);
    presetBankState_.addChild(preset, -1, nullptr);

    setCurrentPresetSelection(slot, name);
    saveStandalonePresetBank();
    return true;
}

bool ECMapperAudioProcessor::deletePresetSlot(const int slot)
{
    {
        const juce::ScopedLock stateGuard(presetStateLock_);
        if (slot < 1 || slot > numPresetSlots)
            return false;

        const auto preset = getPresetNode(slot);
        if (!preset.isValid())
            return false;

        presetBankState_.removeChild(preset, nullptr);

        ensureInitPresetExists();

        if (currentPresetSlot_.load() == slot)
        {
            const auto initSnapshot = getPresetSnapshot(1);
            if (initSnapshot.isValid())
                applyPresetState(initSnapshot);
            setCurrentPresetSelection(1, "Init");
        }

        saveStandalonePresetBank();
    }
    updateGlobalSettings();
    return true;
}

bool ECMapperAudioProcessor::loadPresetSlot(const int slot)
{
    bool success = false;
    {
        const juce::ScopedLock stateGuard(presetStateLock_);
        if (slot < 1 || slot > numPresetSlots)
            return false;

        const auto snapshot = getPresetSnapshot(slot);
        if (snapshot.isValid()) {
            applyPresetState(snapshot);
            const auto preset = getPresetNode(slot);
            auto name = preset.getProperty("name", juce::String()).toString();
            if (name.isEmpty())
                name = "Preset " + juce::String(slot);
            setCurrentPresetSelection(slot, name);
            success = true;
        } else if (slot == 1) {
            ensureInitPresetExists();
            const auto preset = getPresetNode(1);
            auto name = preset.isValid() ? preset.getProperty("name", juce::String()).toString() : juce::String("Init");
            if (name.isEmpty())
                name = "Init";
            const auto initSnapshot = getPresetSnapshot(1);
            if (initSnapshot.isValid())
                applyPresetState(initSnapshot);
            setCurrentPresetSelection(1, name);
            success = true;
        }
    }

    if (success)
        updateGlobalSettings();

    return success;
}

int ECMapperAudioProcessor::transposeIndex(ecm::InstrumentType deviceType, ecm::Zone zone)
{
    const auto deviceIndex = static_cast<int>(deviceType) - 1;
    const auto zoneIndex = static_cast<int>(zone) - 1;
    return deviceIndex * 3 + zoneIndex;
}

bool ECMapperAudioProcessor::isZoneRuntimeParameter(const juce::String& parameterID)
{
    return parameterID.startsWith("transpose_") || parameterID.startsWith("enabled_");
}

void ECMapperAudioProcessor::registerZoneParameterListeners()
{
    for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            state.addParameterListener(ecm::ZoneWrapper::getTransposeParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone)), this);
            state.addParameterListener(ecm::ZoneWrapper::getEnabledParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone)), this);
        }
    }
}

void ECMapperAudioProcessor::unregisterZoneParameterListeners()
{
    for (int device = static_cast<int>(ecm::InstrumentType::Alpha); device <= static_cast<int>(ecm::InstrumentType::Pico); ++device) {
        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            state.removeParameterListener(ecm::ZoneWrapper::getTransposeParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone)), this);
            state.removeParameterListener(ecm::ZoneWrapper::getEnabledParameterID(static_cast<ecm::InstrumentType>(device), static_cast<ecm::Zone>(zone)), this);
        }
    }
}

void ECMapperAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (isZoneRuntimeParameter(parameterID))
        requestRuntimeConfigRefresh();
}

void ECMapperAudioProcessor::deviceListChanged() {}

void ECMapperAudioProcessor::deviceNeedsLEDSync(const std::string& devId, const ecm::InstrumentType type, const bool isRequest) {
    if (hardwareService.getAppRole() == ecm::AppRole::Client) {
        // Only return if Control LEDs is on
        if (hardwareService.isDeviceAuthorizedForLEDs(devId)) {
            midiService.resendLEDs(devId.c_str(), type, &outgoingOSCQueue, isRequest);
        }
    } else if (hardwareService.getAppRole() == ecm::AppRole::Host) {
        // If it's a local device, send to mapperToHardwareQueue
        // We know it's a Host, so if it's not a remote device, it's local.
        bool isRemote = false;
        const auto devices = hardwareService.getConnectedDevices();
        for (const auto& d : devices) {
            if (d.dev == devId) {
                isRemote = d.isRemote;
                break;
            }
        }
        
        if (!isRemote) {
            if (hardwareService.getDeviceMode(devId) == ecm::DeviceMode::Local) {
                midiService.resendLEDs(devId.c_str(), type, &mapperToHardwareQueue, isRequest);
            }
        } else {
            // It's a remote device on a host (which shouldn't happen much, but still)
            midiService.resendLEDs(devId.c_str(), type, &outgoingOSCQueue, isRequest);
        }
    }
}

void ECMapperAudioProcessor::queueKeyboardSelectionMessage(const juce::MidiMessage& message)
{
    const juce::ScopedLock sl(keyboardSelectionLock_);
    keyboardSelectionMessages_.push_back(message);
}

void ECMapperAudioProcessor::drainKeyboardSelectionMessages(std::vector<juce::MidiMessage>& messages)
{
    const juce::ScopedLock sl(keyboardSelectionLock_);
    messages.swap(keyboardSelectionMessages_);
}

void ECMapperAudioProcessor::clearKeyboardSelectionMessages()
{
    const juce::ScopedLock sl(keyboardSelectionLock_);
    keyboardSelectionMessages_.clear();
}

void ECMapperAudioProcessor::ensureInitPresetExists()
{
    const juce::ScopedLock stateGuard(presetStateLock_);
    if (hasPresetSlot(1))
        return;

    const auto snapshot = makeComparableState(state.state.createCopy());
    auto preset = juce::ValueTree("ECMapperPreset");
    preset.setProperty("slot", 1, nullptr);
    preset.setProperty("name", "Init", nullptr);
    preset.addChild(snapshot, -1, nullptr);
    presetBankState_.addChild(preset, -1, nullptr);

    if (currentPresetSlot_.load() == 1)
        currentPresetName_ = "Init";
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ECMapperAudioProcessor();
}
