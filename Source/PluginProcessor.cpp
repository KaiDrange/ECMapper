#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/SettingsWrapper.h"
#include "Core/LayoutWrapper.h"

ECMapperAudioProcessor::ECMapperAudioProcessor() :
    AudioProcessor(BusesProperties()
               .withInput("Input", juce::AudioChannelSet::stereo(), true)
               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "ECMapperState", {}),
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
    
    layoutChangeHandler = std::make_unique<ecm::LayoutChangeHandler>(mapperToHardwareQueue, state.state, configLookups, [this](bool s) { suspendProcessing(s); });
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

    midiMessages.clear();
    
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
    
    if (!layoutChangeHandler->layoutMidiRPNSent) {
        midiService.createLayoutRPNs(midiMessages);
        layoutChangeHandler->layoutMidiRPNSent = true;
    }
    
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ECMapperAudioProcessor();
}
