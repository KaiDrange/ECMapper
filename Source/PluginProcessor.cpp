#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/SettingsWrapper.h"
#include "Core/PresetBankFileUtil.h"
#include "Core/Midi2Protocol.h"
#include "../JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/vst/ivstevents.h"
#include "../JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/vst/ivstmidicontrollers.h"
#include "../JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/vst/ivstnoteexpression.h"
#include <cmath>
#include <string_view>

namespace {

constexpr int kTransposeZone1CcNumber = 22;
constexpr int kTransposeZone3CcNumber = 24;
constexpr int kZone1EnableCcNumber = 25;
constexpr int kZone3EnableCcNumber = 27;
constexpr int kPresetParameterDefaultIndex = 0;
constexpr int kTransposeParameterMinimum = -64;
constexpr int kTransposeParameterMaximum = 63;

bool ecmapperAppendDirectVst3Events(juce::AudioProcessor& processor, Steinberg::Vst::IEventList& outputEvents);

int transposeFromCc(int ccValue)
{
    return juce::jlimit(0, 127, ccValue) - 64;
}

bool enableFromCc(const int ccValue)
{
    return juce::jlimit(0, 127, ccValue) >= 64;
}

juce::String getDeviceGroupId(const ecm::InstrumentType deviceType)
{
    switch (deviceType) {
        case ecm::InstrumentType::Alpha: return "alpha";
        case ecm::InstrumentType::Tau:   return "tau";
        case ecm::InstrumentType::Pico:  return "pico";
        default:                         return "device";
    }
}

juce::String getDeviceDisplayName(const ecm::InstrumentType deviceType)
{
    switch (deviceType) {
        case ecm::InstrumentType::Alpha: return "Alpha";
        case ecm::InstrumentType::Tau:   return "Tau";
        case ecm::InstrumentType::Pico:  return "Pico";
        default:                         return "Device";
    }
}

juce::String getZoneDisplayName(const ecm::Zone zone)
{
    return "Zone " + juce::String(static_cast<int>(zone));
}

class ZoneMidiBufferPerformanceEventSink final : public ecm::PerformanceEventSink
{
public:
    ZoneMidiBufferPerformanceEventSink(std::shared_ptr<ecm::MidiProtocol> protocol,
                                       juce::MidiBuffer& sharedBuffer,
                                       std::array<juce::MidiBuffer, 3>& zoneBuffers)
        : protocol_(std::move(protocol)),
          sharedBuffer_(sharedBuffer),
          zoneBuffers_(zoneBuffers)
    {
    }

    void pushEvent(const ecm::PerformanceEvent& event) override
    {
        if (protocol_ == nullptr)
            return;

        if (event.zoneIndex >= 0 && event.zoneIndex < static_cast<int>(zoneBuffers_.size())) {
            protocol_->renderEvent(zoneBuffers_[static_cast<size_t>(event.zoneIndex)], event);
            return;
        }

        protocol_->renderEvent(sharedBuffer_, event);
    }

private:
    std::shared_ptr<ecm::MidiProtocol> protocol_;
    juce::MidiBuffer& sharedBuffer_;
    std::array<juce::MidiBuffer, 3>& zoneBuffers_;
};

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

juce::ValueTree createPresetSnapshotRoot(const juce::ValueTree& stateTree)
{
    auto snapshot = juce::ValueTree(stateTree.getType());
    snapshot.setProperty(ecm::SettingsWrapper::id_ecMapperVersion, ProjectInfo::versionString, nullptr);

    auto presetTree = stateTree.getChildWithName(ecm::SettingsWrapper::id_preset);
    if (presetTree.isValid()) {
        auto presetCopy = presetTree.createCopy();
        presetCopy.removeProperty(ecm::SettingsWrapper::id_midi2Mode, nullptr);
        presetCopy.removeProperty(ecm::SettingsWrapper::id_pluginOutputMode, nullptr);
        snapshot.addChild(presetCopy, -1, nullptr);
    }

    return snapshot;
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
    ecm::SettingsWrapper::setPluginOutputMode(ecm::SettingsWrapper::getPluginOutputMode(rootState), rootState);
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
        const auto deviceType = static_cast<ecm::InstrumentType>(device);
        auto deviceGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            getDeviceGroupId(deviceType),
            getDeviceDisplayName(deviceType),
            " - ");

        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            const auto zoneType = static_cast<ecm::Zone>(zone);
            auto enabledParamId = ecm::ZoneWrapper::getEnabledParameterID(deviceType, zoneType);
            auto enabledParamName = getZoneDisplayName(zoneType) + " Enable";
            deviceGroup->addChild(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { enabledParamId, 1 },
                enabledParamName,
                true));
        }

        for (int zone = static_cast<int>(ecm::Zone::Zone1); zone <= static_cast<int>(ecm::Zone::Zone3); ++zone) {
            const auto zoneType = static_cast<ecm::Zone>(zone);
            auto paramId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
            auto paramName = getZoneDisplayName(zoneType) + " Transpose";
            deviceGroup->addChild(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID { paramId, 1 },
                paramName,
                kTransposeParameterMinimum,
                kTransposeParameterMaximum,
                0));
        }

        layout.add(std::move(deviceGroup));
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
    juce::ignoreUnused(layouts);
    return true;
}

void ECMapperAudioProcessor::processBlock(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiMessages) {
    audioBuffer.clear();

    const bool useVst3Direct = !juce::JUCEApplicationBase::isStandaloneApp()
                               && ecm::SettingsWrapper::getPluginOutputMode(state.state) == ecm::OutputTransportMode::Vst3Direct;
    const bool useStandaloneZoneRouting = juce::JUCEApplicationBase::isStandaloneApp()
                                          && !ecm::SettingsWrapper::getMidi2Mode(state.state)
                                          && midiService.isStandaloneLegacyZoneRoutingEnabled();
    juce::MidiBuffer* targetBuffer = &midiMessages;
    juce::MidiBuffer tempBuffer;
    std::array<juce::MidiBuffer, 3> zoneBuffers;
    bool useDirect = (ecm::SettingsWrapper::getMidi2Mode(state.state) || midiService.isUsingUMPPath()) 
                     && juce::JUCEApplicationBase::isStandaloneApp();
    useDirect = useDirect || useStandaloneZoneRouting;

    ZoneMidiBufferPerformanceEventSink standaloneZoneSink(midiService.getProtocol(), tempBuffer, zoneBuffers);
    ecm::PerformanceEventSink* eventSink = useVst3Direct
        ? static_cast<ecm::PerformanceEventSink*>(&vst3DirectPerformanceSink_)
        : (useStandaloneZoneRouting ? static_cast<ecm::PerformanceEventSink*>(&standaloneZoneSink) : nullptr);
    
    if (useDirect)
        targetBuffer = &tempBuffer;

    if (useVst3Direct)
        vst3DirectEventQueue_.clear();

    int slotToLoad = -1;
    collectPresetSlotLoadRequests(midiMessages, slotToLoad);
    const auto timing = calculateBlockTiming(audioBuffer);
    if (applyZoneControlMessages(midiMessages))
        requestRuntimeConfigRefresh();
    prepareMidiMessagesForBlock(*targetBuffer);
    processHardwareMessagesForBlock(timing, *targetBuffer, slotToLoad, eventSink);
    if (useVst3Direct)
        midiService.reduceBreath(*targetBuffer, vst3DirectPerformanceSink_, timing.numSamples - 1, timing.numSamples);
    else if (useStandaloneZoneRouting)
        midiService.reduceBreath(*targetBuffer, standaloneZoneSink, timing.numSamples - 1, timing.numSamples);
    else
        midiService.reduceBreath(*targetBuffer, timing.numSamples - 1, timing.numSamples);
    dispatchPresetSlotLoad(slotToLoad);
    
    if (!targetBuffer->isEmpty()) {
        static int debugCounter = 0;
        if (++debugCounter % 100 == 0)
            juce::Logger::writeToLog("PluginProcessor: targetBuffer has " + juce::String(targetBuffer->getNumEvents()) + " events. useDirect=" + juce::String((int)useDirect));
    }

    if (useDirect) {
        if (useStandaloneZoneRouting)
            midiService.sendStandaloneLegacyMidiBuffers(tempBuffer, zoneBuffers);
        else
            midiService.drainDirectUMPs(tempBuffer);
        midiMessages.clear();
    }
    
    midiService.finishedBlock();
}

bool ECMapperAudioProcessor::isVst3DirectOutputEnabled() const
{
    return !juce::JUCEApplicationBase::isStandaloneApp()
        && ecm::SettingsWrapper::getPluginOutputMode(const_cast<juce::ValueTree&>(state.state)) == ecm::OutputTransportMode::Vst3Direct;
}

std::vector<ecm::Vst3DirectEvent> ECMapperAudioProcessor::drainPendingVst3DirectEvents()
{
    return vst3DirectEventQueue_.drain();
}

void ECMapperAudioProcessor::clearPendingVst3DirectEvents()
{
    vst3DirectEventQueue_.clear();
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

    midiService.drainPendingMidiMessages(midiMessages, 0);
}

void ECMapperAudioProcessor::processHardwareMessagesForBlock(const BlockTiming& timing, juce::MidiBuffer& midiMessages, int& slotToLoad, ecm::PerformanceEventSink* sink)
{
    ecm::osc::Message msg;

    while (hardwareToMapperQueue.read(msg)) {
        handleHardwareMessage(msg, timing, midiMessages, slotToLoad, sink);
    }
}

void ECMapperAudioProcessor::handleHardwareMessage(const ecm::osc::Message& msg, const BlockTiming& timing, juce::MidiBuffer& midiMessages, int& slotToLoad, ecm::PerformanceEventSink* sink)
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
    if (sink != nullptr)
        midiService.processMessage(msg, outgoingMsg, midiMessages, *sink, sampleOffset, &presetSlotRequest);
    else
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
    midiService.setRuntimeConfigSnapshot(std::make_unique<ecm::MidiService::RuntimeConfigSnapshot>(configLookups, midiService.getProtocol(), midiService.getVoiceRouter(), midiService.getExpressionPolicy()));
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

        const auto controllerNumber = msg.getControllerNumber();
        const int targetZone = (controllerNumber >= kTransposeZone1CcNumber && controllerNumber <= kTransposeZone3CcNumber)
                                 ? controllerNumber - kTransposeZone1CcNumber + static_cast<int>(ecm::Zone::Zone1)
                                 : (controllerNumber >= kZone1EnableCcNumber && controllerNumber <= kZone3EnableCcNumber)
                                       ? controllerNumber - kZone1EnableCcNumber + static_cast<int>(ecm::Zone::Zone1)
                                       : 0;
        if (targetZone == 0)
            continue;

        const int firstDevice = channel == 1 ? static_cast<int>(ecm::InstrumentType::Alpha)
                                              : channel - 1;
        const int lastDevice = channel == 1 ? static_cast<int>(ecm::InstrumentType::Pico)
                                             : channel - 1;

        if (controllerNumber >= kTransposeZone1CcNumber && controllerNumber <= kTransposeZone3CcNumber) {
            const auto transposeValue = transposeFromCc(msg.getControllerValue());
            for (int device = firstDevice; device <= lastDevice; ++device) {
                const auto deviceType = static_cast<ecm::InstrumentType>(device);
                const auto zoneType = static_cast<ecm::Zone>(targetZone);
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
            continue;
        }

        const bool enabled = enableFromCc(msg.getControllerValue());
        for (int device = firstDevice; device <= lastDevice; ++device) {
            const auto deviceType = static_cast<ecm::InstrumentType>(device);
            const auto zoneType = static_cast<ecm::Zone>(targetZone);
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
    auto stateCopy = ecm::SettingsWrapper::createPersistentStateTree(state.state);
    auto presetBankCopy = ecm::PresetBankFileUtil::createExportTree(presetBankState_);
    bundle.addChild(stateCopy, -1, nullptr);
    bundle.addChild(presetBankCopy, -1, nullptr);
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
            if (liveState.isValid()) {
                mergeTreeIntoLive(state.state, liveState);
                ecm::SettingsWrapper::normalizeStateTree(state.state);
            }

            const auto bankState = tree.getChildWithName(presetBankState_.getType());
            if (bankState.isValid()) {
                presetBankState_ = bankState;
                ecm::PresetBankFileUtil::normalizePresetBankState(presetBankState_);
            }
        } else if (tree.hasType(state.state.getType())) {
            const juce::ScopedValueSetter batchGuard(presetBatchInProgress_, true);
            mergeTreeIntoLive(state.state, tree);
            ecm::SettingsWrapper::normalizeStateTree(state.state);
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

        if (!hardwareService.isServiceRunning()) {
            const auto resolvedRole = ecm::HardwareService::resolveStartupAppRole(role,
                                                                                  ecm::OSCBridge::isPortOccupied(12121));

            if (resolvedRole != role)
                logger.log(resolvedRole == ecm::AppRole::Client
                               ? "Host detected on network (port 12121 busy). Auto-switching to Client mode."
                               : "Discovery port 12121 is free. Auto-switching to Host mode for the first instance.");

            role = resolvedRole;
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
    if (snapshot.isValid()) {
        ecm::SettingsWrapper::normalizeStateTree(snapshot);
        return snapshot;
    }

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

    auto snapshotCopy = snapshot.createCopy();
    if (auto presetTree = snapshotCopy.getChildWithName(ecm::SettingsWrapper::id_preset); presetTree.isValid()) {
        presetTree.removeProperty(ecm::SettingsWrapper::id_midi2Mode, nullptr);
        presetTree.removeProperty(ecm::SettingsWrapper::id_pluginOutputMode, nullptr);
    }

    const juce::ScopedLock stateGuard(presetStateLock_);
    {
        const juce::ScopedValueSetter batchGuard(presetBatchInProgress_, true);
        auto& liveState = state.state;
        ecm::SettingsWrapper::normalizeStateTree(liveState);
        mergeTreeIntoLive(liveState, snapshotCopy);
        ecm::SettingsWrapper::normalizeStateTree(liveState);

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

    ecm::SettingsWrapper::normalizeStateTree(stateTree);
    materializePresetState(stateTree);

    return createPresetSnapshotRoot(stateTree);
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

    auto importResult = ecm::PresetBankFileUtil::readPresetBankFile(file);
    if (!importResult.presetBank.isValid())
        return;

    presetBankState_ = std::move(importResult.presetBank);

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

    ecm::PresetBankFileUtil::writePresetBankFile(file, presetBankState_);
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

bool ECMapperAudioProcessor::importPresetBankFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto importResult = ecm::PresetBankFileUtil::readPresetBankFile(file);
    if (!importResult.presetBank.isValid())
        return false;

    juce::ValueTree snapshotToApply;
    juce::String presetName;
    int slotToLoad = 1;

    {
        const juce::ScopedLock stateGuard(presetStateLock_);
        const auto preferredSlot = juce::jlimit(1, numPresetSlots, currentPresetSlot_.load());

        presetBankState_ = std::move(importResult.presetBank);
        ensureInitPresetExists();

        slotToLoad = hasPresetSlot(preferredSlot) ? preferredSlot : 1;

        auto preset = getPresetNode(slotToLoad);
        snapshotToApply = getPresetSnapshot(slotToLoad).createCopy();
        presetName = preset.isValid() ? preset.getProperty("name", juce::String()).toString()
                                      : juce::String();

        if (presetName.isEmpty())
            presetName = slotToLoad == 1 ? juce::String("Init") : "Preset " + juce::String(slotToLoad);

        saveStandalonePresetBank();
    }

    if (!snapshotToApply.isValid())
        return false;

    applyPresetState(snapshotToApply);
    setCurrentPresetSelection(slotToLoad, presetName);
    updateGlobalSettings();
    return true;
}

bool ECMapperAudioProcessor::exportPresetBankToFile(const juce::File& file) const
{
    if (file == juce::File())
        return false;

    if (!file.getParentDirectory().exists())
        // ReSharper disable once CppExpressionWithoutSideEffects
        file.getParentDirectory().createDirectory();

    juce::ValueTree bankCopy;
    {
        const juce::ScopedLock stateGuard(presetStateLock_);
        bankCopy = presetBankState_.createCopy();
        ecm::PresetBankFileUtil::normalizePresetBankState(bankCopy);
    }

    return ecm::PresetBankFileUtil::writePresetBankFile(file, bankCopy);
}

std::size_t ECMapperAudioProcessor::transposeIndex(ecm::InstrumentType deviceType, ecm::Zone zone)
{
    const auto deviceIndex = static_cast<std::size_t>(static_cast<int>(deviceType) - 1);
    const auto zoneIndex = static_cast<std::size_t>(static_cast<int>(zone) - 1);
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
        if (hardwareService.isDeviceAuthorizedForLEDs(devId)) {
            midiService.resendLEDs(devId.c_str(), type, &outgoingOSCQueue, isRequest);
        }
    } else if (hardwareService.getAppRole() == ecm::AppRole::Host) {
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

bool ecmapperAppendDirectVst3Events(juce::AudioProcessor& processor, Steinberg::Vst::IEventList& outputEvents)
{
    auto* ecMapperProcessor = dynamic_cast<ECMapperAudioProcessor*>(&processor);
    if (ecMapperProcessor == nullptr || !ecMapperProcessor->isVst3DirectOutputEnabled())
        return false;

    auto pendingEvents = ecMapperProcessor->drainPendingVst3DirectEvents();

    using namespace Steinberg::Vst;

    for (const auto& sourceEvent : pendingEvents)
    {
        Event event {};
        event.busIndex = 0;
        event.sampleOffset = sourceEvent.sampleOffset;
        event.ppqPosition = 0.0;
        event.flags = 0;

        switch (sourceEvent.kind)
        {
            case ecm::Vst3DirectEventKind::NoteOn:
                event.type = Event::kNoteOnEvent;
                event.noteOn.channel = static_cast<int16>(sourceEvent.channel);
                event.noteOn.pitch = static_cast<int16>(sourceEvent.noteNumber);
                event.noteOn.tuning = 0.0f;
                event.noteOn.velocity = static_cast<float>(sourceEvent.value);
                event.noteOn.length = 0;
                event.noteOn.noteId = sourceEvent.noteId;
                break;

            case ecm::Vst3DirectEventKind::NoteOff:
                event.type = Event::kNoteOffEvent;
                event.noteOff.channel = static_cast<int16>(sourceEvent.channel);
                event.noteOff.pitch = static_cast<int16>(sourceEvent.noteNumber);
                event.noteOff.velocity = static_cast<float>(sourceEvent.value);
                event.noteOff.noteId = sourceEvent.noteId;
                event.noteOff.tuning = 0.0f;
                break;

            case ecm::Vst3DirectEventKind::PolyPressure:
                event.type = Event::kPolyPressureEvent;
                event.polyPressure.channel = static_cast<int16>(sourceEvent.channel);
                event.polyPressure.pitch = static_cast<int16>(sourceEvent.noteNumber);
                event.polyPressure.noteId = sourceEvent.noteId;
                event.polyPressure.pressure = static_cast<float>(sourceEvent.value);
                break;

            case ecm::Vst3DirectEventKind::NoteExpression:
                event.type = Event::kNoteExpressionValueEvent;
                event.noteExpressionValue.noteId = sourceEvent.noteId;
                switch (sourceEvent.expressionType)
                {
                    case ecm::Vst3NoteExpressionType::Tuning:
                        event.noteExpressionValue.typeId = NoteExpressionTypeIDs::kTuningTypeID;
                        break;
                    case ecm::Vst3NoteExpressionType::Brightness:
                        event.noteExpressionValue.typeId = NoteExpressionTypeIDs::kBrightnessTypeID;
                        break;
                    case ecm::Vst3NoteExpressionType::Expression:
                    default:
                        event.noteExpressionValue.typeId = NoteExpressionTypeIDs::kExpressionTypeID;
                        break;
                }
                event.noteExpressionValue.value = sourceEvent.value;
                break;

            case ecm::Vst3DirectEventKind::LegacyCC:
                event.type = Event::kLegacyMIDICCOutEvent;
                event.midiCCOut.channel = static_cast<int8>(sourceEvent.channel);
                event.midiCCOut.controlNumber = static_cast<uint8>(sourceEvent.controller);
                event.midiCCOut.value = static_cast<int8>(juce::jlimit(0, 127, juce::roundToInt(sourceEvent.value * 127.0)));
                event.midiCCOut.value2 = static_cast<int8>(juce::jlimit(0, 127, juce::roundToInt(sourceEvent.value2 * 127.0)));
                break;
        }

        outputEvents.addEvent(event);
    }

    return !pendingEvents.empty();
}
