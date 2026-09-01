#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/SettingsWrapper.h"
#include <cmath>

namespace {

constexpr int kTransposeCcNumber = 22;
constexpr int kZoneEnableCcNumber = 23;
constexpr int kPresetParameterDefaultIndex = 0;

struct DeviceKeyCounts
{
    int normal = 0;
    int perc = 0;
    int buttons = 0;
};

DeviceKeyCounts getDeviceKeyCounts(ecm::InstrumentType deviceType)
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
        || property == ecm::SettingsWrapper::id_upperMPEPB;
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
}

void materializeLayoutKeysForDevice(ecm::InstrumentType deviceType, juce::ValueTree& rootState)
{
    auto counts = getDeviceKeyCounts(deviceType);

    auto materializeKey = [&](ecm::LayoutWrapper::KeyId keyId)
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

void materializeZoneState(ecm::InstrumentType deviceType, ecm::Zone zone, juce::ValueTree& rootState)
{
    ecm::ZoneWrapper::setEnabled(deviceType, zone, ecm::ZoneWrapper::getEnabled(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setTranspose(deviceType, zone, ecm::ZoneWrapper::getTranspose(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setKeyPitchbend(deviceType, zone, ecm::ZoneWrapper::getKeyPitchbend(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setChannelMaxPitchbend(deviceType, zone, ecm::ZoneWrapper::getChannelMaxPitchbend(deviceType, zone, rootState), rootState);
    ecm::ZoneWrapper::setMidiChannelType(deviceType, zone, ecm::ZoneWrapper::getMidiChannelType(deviceType, zone, rootState), rootState);

    auto setMidiValue = [&](juce::Identifier childId, ecm::ZoneWrapper::MidiValue defaultValue)
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
    for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
        auto deviceType = static_cast<ecm::InstrumentType>(device);
        materializeLayoutKeysForDevice(deviceType, rootState);
        for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone)
            materializeZoneState(deviceType, static_cast<ecm::Zone>(zone), rootState);
        materializeCurveState(deviceType, rootState);
    }

    ecm::SettingsWrapper::setLowerMPEVoiceCount(ecm::SettingsWrapper::getLowerMPEVoiceCount(rootState), rootState);
    ecm::SettingsWrapper::setUpperMPEVoiceCount(ecm::SettingsWrapper::getUpperMPEVoiceCount(rootState), rootState);
    ecm::SettingsWrapper::setLowerMPEPB(ecm::SettingsWrapper::getLowerMPEPB(rootState), rootState);
    ecm::SettingsWrapper::setUpperMPEPB(ecm::SettingsWrapper::getUpperMPEPB(rootState), rootState);
}

void mergeTreeIntoLive(juce::ValueTree& liveTree, const juce::ValueTree& snapshotTree)
{
    if (!liveTree.isValid() || !snapshotTree.isValid())
        return;

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

    for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
        for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
            auto paramId = ecm::ZoneWrapper::getTransposeParameterID((ecm::InstrumentType) device, (ecm::Zone) zone);
            auto paramName = juce::String::formatted("Transpose %d-%d", device, zone);
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { paramId, 1 }, paramName, -96, 96, 0));

            auto enabledParamId = ecm::ZoneWrapper::getEnabledParameterID((ecm::InstrumentType) device, (ecm::Zone) zone);
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
        ecm::ConfigLookup(ecm::InstrumentType::Alpha, state), 
        ecm::ConfigLookup(ecm::InstrumentType::Tau, state), 
        ecm::ConfigLookup(ecm::InstrumentType::Pico, state) 
    },
    hardwareService(hardwareToMapperQueue, mapperToHardwareQueue),
    midiService(configLookups),
    oscBridge(hardwareService, hardwareToMapperQueue, mapperToHardwareQueue, outgoingOSCQueue, logger) {
    
    hardwareService.addListener(this);
    hardwareService.setOSCBroadcastQueue(&outgoingOSCQueue);
    midiService.setOSCBroadcastQueue(&outgoingOSCQueue);

    presetSlotParameter_ = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(presetSlotParameterId));
    jassert(presetSlotParameter_ != nullptr);
    factoryDefaultState_ = makeComparableState(state.state.createCopy());
    currentPresetState_ = factoryDefaultState_.createCopy();
    currentPresetSlot_ = 1;
    currentPresetName_ = "Default";
    lastPresetParameterIndex_ = presetSlotParameter_ != nullptr ? presetSlotParameter_->getIndex() : 0;
    
    layoutChangeHandler = std::make_unique<ecm::LayoutChangeHandler>(
        mapperToHardwareQueue,
        state.state,
        configLookups,
        [this](bool s) { suspendProcessing(s); },
        [this](ecm::InstrumentType deviceType, ecm::Zone zone) { midiService.queueTransposeChangeFlush(deviceType, zone); });
    state.state.addListener(layoutChangeHandler.get());
}

ECMapperAudioProcessor::~ECMapperAudioProcessor() {
    hardwareService.removeListener(this);
    state.state.removeListener(layoutChangeHandler.get());
}

void ECMapperAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    logger.log("prepareToPlay() called.");
    
    updateGlobalSettings();
    juce::MidiBuffer emptyMidi;
    syncZoneParameters(emptyMidi);
    midiService.start(state, &hardwareService);
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

    int numSamples = audioBuffer.getNumSamples();
    double sampleRate = getSampleRate();
    double blockDurationUs = 1000000.0 * numSamples / sampleRate;
    double nowUs = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()) * 1000000.0;
    
    // If this is the first block, or if there's a huge gap, reset our timeline
    if (lastBlockEndUs == 0.0 || std::abs(nowUs - lastBlockEndUs) > 500000.0) {
        lastBlockEndUs = nowUs - blockDurationUs;
        localClockOffset = 0.0;
        remoteClockOffsets.clear();
    }
    
    double blockStartUs = lastBlockEndUs;
    double blockEndUs = blockStartUs + blockDurationUs;
    
    // Update for next time
    lastBlockEndUs = blockEndUs;

    if (presetSlotParameter_ != nullptr) {
        auto selectedIndex = presetSlotParameter_->getIndex();
        if (ignorePresetParameterUpdate_) {
            lastPresetParameterIndex_ = selectedIndex;
            currentPresetSlot_ = juce::jlimit(1, numPresetSlots, selectedIndex + 1);
            auto presetNode = getPresetNode(currentPresetSlot_);
            currentPresetName_ = presetNode.isValid() ? presetNode.getProperty("name", juce::String()).toString()
                                                      : (currentPresetSlot_ == 1 ? juce::String("Default") : juce::String("Empty"));
            ignorePresetParameterUpdate_ = false;
        } else if (selectedIndex != lastPresetParameterIndex_) {
            lastPresetParameterIndex_ = selectedIndex;
            loadPresetSlot(selectedIndex + 1);
        }
    }

    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        if (msg.isProgramChange()) {
            auto slot = msg.getProgramChangeNumber() + 1;
            if (slot >= 1 && slot <= numPresetSlots)
                loadPresetSlot(slot);
        }
    }

    syncZoneParameters(midiMessages);

    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn() || msg.isNoteOff())
            queueKeyboardSelectionMessage(msg);
    }

    midiMessages.clear();
    
    if (!layoutChangeHandler->layoutMidiRPNSent) {
        midiService.createLayoutRPNs(midiMessages);
        layoutChangeHandler->layoutMidiRPNSent = true;
    }

    midiService.drainPendingMidiMessages(midiMessages, 0);
    
    ecm::osc::Message msg;
    ecm::osc::Message outgoingMsg;
    
    while (hardwareToMapperQueue.read(msg)) {
        if (msg.type == ecm::osc::MessageType::Device) {
            layoutChangeHandler->sendLEDMsgForAllKeys(msg.device);
        } else {
            int sampleOffset = 0;
            if (msg.timestamp > 0) {
                double localMsgTime = static_cast<double>(msg.timestamp);
                
                if (msg.isRemote) {
                    juce::String devId(msg.devId);
                    if (remoteClockOffsets.find(devId) == remoteClockOffsets.end()) {
                        // Initialize offset for this remote device
                        // We assume the message just arrived, so its local time is 'now'
                        remoteClockOffsets[devId] = nowUs - static_cast<double>(msg.timestamp);
                    }
                    localMsgTime += remoteClockOffsets[devId];
                } else {
                    if (localClockOffset == 0.0) {
                        localClockOffset = nowUs - static_cast<double>(msg.timestamp);
                    }
                    localMsgTime += localClockOffset;
                }

                // Map hardware timestamp to sample offset within the block
                double offsetUs = localMsgTime - blockStartUs;
                sampleOffset = static_cast<int>(offsetUs * sampleRate / 1000000.0);
                sampleOffset = std::clamp(sampleOffset, 0, numSamples - 1);
            }

            outgoingMsg.type = ecm::osc::MessageType::Undefined;
            midiService.processMessage(msg, outgoingMsg, midiMessages, sampleOffset);
            if (outgoingMsg.type == ecm::osc::MessageType::LED) {
                if (hardwareService.getDeviceMode(msg.devId) == ecm::DeviceMode::Local) {
                    mapperToHardwareQueue.add(outgoingMsg);
                }
            }
        }
    }
    
    midiService.reduceBreath(midiMessages, numSamples - 1);
}

juce::AudioProcessorEditor* ECMapperAudioProcessor::createEditor() {
    return new ECMapperAudioProcessorEditor(*this);
}

void ECMapperAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree bundle("ECMapperStateBundle");
    bundle.addChild(state.copyState(), -1, nullptr);
    bundle.addChild(presetBankState_.createCopy(), -1, nullptr);
    std::unique_ptr<juce::XmlElement> xml(bundle.createXml());
    copyXmlToBinary(*xml, destData);
}

void ECMapperAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml(*xmlState);
    if (!tree.isValid())
        return;

    if (tree.hasType("ECMapperStateBundle")) {
        auto liveState = tree.getChildWithName(state.state.getType());
        if (liveState.isValid())
            state.replaceState(liveState);

        auto bankState = tree.getChildWithName(presetBankState_.getType());
        if (bankState.isValid())
            presetBankState_ = bankState;
    } else if (tree.hasType(state.state.getType())) {
        state.replaceState(tree);
    }

    presetSlotParameter_ = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(presetSlotParameterId));
    lastPresetParameterIndex_ = presetSlotParameter_ != nullptr ? presetSlotParameter_->getIndex() : 0;
    currentPresetSlot_ = juce::jlimit(1, numPresetSlots, lastPresetParameterIndex_ + 1);
    {
        auto presetNode = getPresetNode(currentPresetSlot_);
        currentPresetName_ = presetNode.isValid() ? presetNode.getProperty("name", juce::String()).toString()
                                                  : (currentPresetSlot_ == 1 ? juce::String("Default") : juce::String("Empty"));
    }
    currentPresetState_ = makeComparableState(state.state.createCopy());
    ignorePresetParameterUpdate_ = true;
    updateGlobalSettings();
}

void ECMapperAudioProcessor::updateGlobalSettings() {
    auto role = ecm::SettingsWrapper::getAppRole(state.state);

    if (!ecm::HardwareService::supportsLocalHardware()) {
        role = ecm::AppRole::Client;
    } else if (role == ecm::AppRole::Host && ecm::OSCBridge::isPortOccupied(12121)) {
        logger.log("Host detected on network (port 12121 busy). Auto-switching to Client mode.");
        role = ecm::AppRole::Client;
    }

    hardwareService.setAppRole(role);
    hardwareService.setClientListenSettings(ecm::SettingsWrapper::getClientListenIP(state.state), 
                                         ecm::SettingsWrapper::getClientListenPort(state.state));
}

juce::AudioProcessorValueTreeState::ParameterLayout ECMapperAudioProcessor::createParameterLayout()
{
    return createTransposeParameters();
}

juce::String ECMapperAudioProcessor::getCurrentPresetName() const
{
    return currentPresetName_;
}

juce::String ECMapperAudioProcessor::getCurrentPresetDisplayName() const
{
    return getPresetSlotDisplayName(currentPresetSlot_);
}

juce::String ECMapperAudioProcessor::getPresetSlotDisplayName(int slot) const
{
    if (slot < 1 || slot > numPresetSlots)
        return {};

    auto label = juce::String(slot) + ": ";
    auto preset = getPresetNode(slot);
    auto presetName = preset.isValid() ? preset.getProperty("name", juce::String()).toString() : juce::String();

    if (slot == 1)
    {
        if (presetName.isEmpty())
            presetName = "Default";

        return label + presetName + " (default)";
    }

    if (preset.isValid()) {
        if (presetName.isEmpty())
            presetName = "Preset";
        return label + presetName;
    }

    return label + "Empty";
}

bool ECMapperAudioProcessor::hasPresetSlot(int slot) const
{
    if (slot < 1 || slot > numPresetSlots)
        return false;

    auto slotValue = slot;
    for (int i = 0; i < presetBankState_.getNumChildren(); ++i) {
        auto preset = presetBankState_.getChild(i);
        if ((int) preset.getProperty("slot", 0) == slotValue)
            return true;
    }

    return false;
}

bool ECMapperAudioProcessor::hasUnsavedChanges() const
{
    auto liveState = makeComparableState(state.state.createCopy());
    if (!currentPresetState_.isValid())
        return true;

    return liveState.toXmlString() != currentPresetState_.toXmlString();
}

juce::ValueTree ECMapperAudioProcessor::getPresetNode(int slot) const
{
    if (slot < 1 || slot > numPresetSlots)
        return {};

    for (int i = 0; i < presetBankState_.getNumChildren(); ++i) {
        auto preset = presetBankState_.getChild(i);
        if ((int) preset.getProperty("slot", 0) == slot)
            return preset;
    }

    return {};
}

juce::ValueTree ECMapperAudioProcessor::getPresetSnapshot(int slot) const
{
    auto preset = getPresetNode(slot);
    if (!preset.isValid())
        return {};

    auto snapshot = preset.getChildWithName(state.state.getType());
    if (snapshot.isValid())
        return snapshot;

    return {};
}

void ECMapperAudioProcessor::setCurrentPresetSelection(int slot, const juce::String& name)
{
    currentPresetSlot_ = juce::jlimit(1, numPresetSlots, slot);
    currentPresetName_ = name;
    currentPresetState_ = makeComparableState(state.state.createCopy());

    if (presetSlotParameter_ != nullptr) {
        auto index = currentPresetSlot_ - 1;
        if (presetSlotParameter_->getIndex() != index) {
            const juce::ScopedValueSetter<bool> guard(ignorePresetParameterUpdate_, true);
            presetSlotParameter_->setValueNotifyingHost(presetSlotParameter_->convertTo0to1((float) index));
        }
        lastPresetParameterIndex_ = index;
    }
}

void ECMapperAudioProcessor::applyPresetState(const juce::ValueTree& snapshot)
{
    if (!snapshot.isValid())
        return;

    auto& liveState = state.state;
    mergeTreeIntoLive(liveState, snapshot);

    auto snapshotGlobalSettings = snapshot.getChildWithName(ecm::SettingsWrapper::id_globalSettings);
    if (snapshotGlobalSettings.isValid()) {
        applyGlobalSettingsPreset(liveState, snapshotGlobalSettings);
    } else {
        applyGlobalSettingsPreset(liveState, juce::ValueTree());
    }

    for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
        for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
            auto deviceType = static_cast<ecm::InstrumentType>(device);
            auto zoneType = static_cast<ecm::Zone>(zone);

            auto transposeId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
            if (auto* param = dynamic_cast<juce::AudioParameterInt*>(state.getParameter(transposeId))) {
                auto transposeValue = ecm::ZoneWrapper::getTranspose(deviceType, zoneType, liveState);
                param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) transposeValue));
            }

            auto enabledId = ecm::ZoneWrapper::getEnabledParameterID(deviceType, zoneType);
            if (auto* param = dynamic_cast<juce::AudioParameterBool*>(state.getParameter(enabledId))) {
                auto enabledValue = ecm::ZoneWrapper::getEnabled(deviceType, zoneType, liveState);
                param->setValueNotifyingHost(enabledValue ? 1.0f : 0.0f);
            }
        }
    }
}

void ECMapperAudioProcessor::resetPresetBankToDefault()
{
    presetBankState_.removeAllChildren(nullptr);
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

void ECMapperAudioProcessor::loadStandalonePresetBank()
{
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        return;

    auto file = getStandalonePresetBankFile();
    if (!file.existsAsFile()) {
        resetPresetBankToDefault();
        return;
    }

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return;

    auto bundle = juce::ValueTree::fromXml(*xml);
    if (!bundle.isValid())
        return;

    if (bundle.hasType("ECMapperPresetBank")) {
        presetBankState_ = bundle;
    } else if (bundle.hasType("ECMapperStateBundle")) {
        auto bank = bundle.getChildWithName(presetBankState_.getType());
        if (bank.isValid())
            presetBankState_ = bank;
    }
}

void ECMapperAudioProcessor::saveStandalonePresetBank() const
{
    if (!juce::JUCEApplicationBase::isStandaloneApp())
        return;

    auto file = getStandalonePresetBankFile();
    if (!file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();

    juce::ValueTree bankCopy = presetBankState_.createCopy();
    std::unique_ptr<juce::XmlElement> xml(bankCopy.createXml());
    if (xml != nullptr)
        xml->writeTo(file);
}

juce::File ECMapperAudioProcessor::getStandalonePresetBankFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ECMapper")
        .getChildFile("preset_bank.xml");
}

bool ECMapperAudioProcessor::savePresetSlot(int slot, const juce::String& name)
{
    if (slot < 1 || slot > numPresetSlots)
        return false;

    auto snapshot = makeComparableState(state.state.createCopy());

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

bool ECMapperAudioProcessor::deletePresetSlot(int slot)
{
    if (slot < 1 || slot > numPresetSlots)
        return false;

    auto preset = getPresetNode(slot);
    if (!preset.isValid())
        return false;

    presetBankState_.removeChild(preset, nullptr);

    if (currentPresetSlot_ == slot)
    {
        applyPresetState(factoryDefaultState_);
        setCurrentPresetSelection(1, "Default");
        updateGlobalSettings();
    }

    saveStandalonePresetBank();
    return true;
}

bool ECMapperAudioProcessor::loadPresetSlot(int slot)
{
    if (slot < 1 || slot > numPresetSlots)
        return false;

    auto snapshot = getPresetSnapshot(slot);
    if (snapshot.isValid()) {
        applyPresetState(snapshot);
        auto preset = getPresetNode(slot);
        auto name = preset.getProperty("name", juce::String()).toString();
        if (name.isEmpty())
            name = "Preset " + juce::String(slot);
        setCurrentPresetSelection(slot, name);
        updateGlobalSettings();
        return true;
    }

    if (slot == 1) {
        applyPresetState(factoryDefaultState_);
        setCurrentPresetSelection(1, "Default");
        updateGlobalSettings();
        return true;
    }

    applyPresetState(factoryDefaultState_);
    setCurrentPresetSelection(slot, "Empty");
    updateGlobalSettings();
    return true;
}

static int transposeFromCc(int ccValue)
{
    ccValue = juce::jlimit(0, 127, ccValue);

    if (ccValue == 64)
        return 0;

    if (ccValue < 64)
        return juce::jlimit(-96, 0, juce::roundToInt(juce::jmap((float) ccValue, 0.0f, 63.0f, -96.0f, -1.0f)));

    return juce::jlimit(0, 96, juce::roundToInt(juce::jmap((float) ccValue, 65.0f, 127.0f, 1.0f, 96.0f)));
}

static bool enableFromCc(int ccValue)
{
    return juce::jlimit(0, 127, ccValue) >= 64;
}

int ECMapperAudioProcessor::transposeIndex(ecm::InstrumentType deviceType, ecm::Zone zone)
{
    return ((int) deviceType - 1) * 3 + ((int) zone - 1);
}

void ECMapperAudioProcessor::syncZoneParameters(juce::MidiBuffer& midiMessages)
{
    if (!transposeCacheInitialised_) {
        for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
            for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
                auto idx = transposeIndex((ecm::InstrumentType) device, (ecm::Zone) zone);
                if (auto* raw = state.getRawParameterValue(ecm::ZoneWrapper::getTransposeParameterID((ecm::InstrumentType) device, (ecm::Zone) zone)))
                    transposeCache_[idx] = (int) std::lround(raw->load());
            }
        }
        transposeCacheInitialised_ = true;
    }

    if (!enableCacheInitialised_) {
        for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
            for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
                auto idx = transposeIndex((ecm::InstrumentType) device, (ecm::Zone) zone);
                if (auto* raw = state.getRawParameterValue(ecm::ZoneWrapper::getEnabledParameterID((ecm::InstrumentType) device, (ecm::Zone) zone)))
                    enableCache_[idx] = raw->load() > 0.5f ? 1 : 0;
            }
        }
        enableCacheInitialised_ = true;
    }

    bool deviceNeedsUpdate[3] = { false, false, false };

    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        if (!msg.isController())
            continue;

        auto channel = msg.getChannel();
        if (channel < 1 || channel > 4)
            continue;

        if (msg.getControllerNumber() == kTransposeCcNumber) {
            for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
                for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
                    if (channel != 4 && zone != channel)
                        continue;

                    auto transposeValue = transposeFromCc(msg.getControllerValue());
                    auto paramId = ecm::ZoneWrapper::getTransposeParameterID((ecm::InstrumentType) device, (ecm::Zone) zone);
                    if (auto* param = dynamic_cast<juce::AudioParameterInt*>(state.getParameter(paramId)))
                        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) transposeValue));
                }
            }
            continue;
        }

        if (msg.getControllerNumber() != kZoneEnableCcNumber)
            continue;

        bool enabled = enableFromCc(msg.getControllerValue());

        for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
            for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
                if (channel != 4 && zone != channel)
                    continue;

                auto paramId = ecm::ZoneWrapper::getEnabledParameterID((ecm::InstrumentType) device, (ecm::Zone) zone);
                if (auto* param = dynamic_cast<juce::AudioParameterBool*>(state.getParameter(paramId)))
                    param->setValueNotifyingHost(enabled);
            }
        }
    }

    for (int device = (int) ecm::InstrumentType::Alpha; device <= (int) ecm::InstrumentType::Pico; ++device) {
        for (int zone = (int) ecm::Zone::Zone1; zone <= (int) ecm::Zone::Zone3; ++zone) {
            auto deviceType = (ecm::InstrumentType) device;
            auto zoneType = (ecm::Zone) zone;
            auto idx = transposeIndex(deviceType, zoneType);
            auto paramId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zoneType);
            int currentValue = transposeCache_[idx];
            if (auto* raw = state.getRawParameterValue(paramId))
                currentValue = (int) std::lround(raw->load());

            if (currentValue != transposeCache_[idx]) {
                midiService.queueTransposeChangeFlush(deviceType, zoneType);
                transposeCache_[idx] = currentValue;
                deviceNeedsUpdate[device - 1] = true;
            }

            int enabledValue = enableCache_[idx];
            if (auto* raw = state.getRawParameterValue(ecm::ZoneWrapper::getEnabledParameterID(deviceType, zoneType)))
                enabledValue = raw->load() > 0.5f ? 1 : 0;

            if (enabledValue != enableCache_[idx]) {
                auto enabled = enabledValue != 0;
                if (ecm::ZoneWrapper::getEnabled(deviceType, zoneType, state.state) != enabled)
                    ecm::ZoneWrapper::setEnabled(deviceType, zoneType, enabled, state.state);
                enableCache_[idx] = enabledValue;
                deviceNeedsUpdate[device - 1] = true;
            }
        }
    }

    for (int device = 0; device < 3; ++device) {
        if (deviceNeedsUpdate[device])
            configLookups[device].updateAll();
    }
}

void ECMapperAudioProcessor::deviceListChanged() {}

void ECMapperAudioProcessor::deviceNeedsLEDSync(const std::string& devId, ecm::InstrumentType type, bool isRequest) {
    if (hardwareService.getAppRole() == ecm::AppRole::Client) {
        // Only return if Control LEDs is on
        if (hardwareService.isDeviceAuthorizedForLEDs(devId)) {
            midiService.resendLEDs(devId.c_str(), type, &outgoingOSCQueue, isRequest);
        }
    } else if (hardwareService.getAppRole() == ecm::AppRole::Host) {
        // If it's a local device, send to mapperToHardwareQueue
        // We know it's a Host, so if it's not a remote device, it's local.
        bool isRemote = false;
        auto devices = hardwareService.getConnectedDevices();
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ECMapperAudioProcessor();
}
