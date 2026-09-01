#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/SettingsWrapper.h"
#include <cmath>

namespace {

constexpr int kTransposeCcNumber = 22;
constexpr int kZoneEnableCcNumber = 23;

juce::AudioProcessorValueTreeState::ParameterLayout createTransposeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

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
    auto stateTree = state.copyState();
    std::unique_ptr<juce::XmlElement> xml(stateTree.createXml());
    copyXmlToBinary(*xml, destData);
}

void ECMapperAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(state.state.getType())) {
        state.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
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
