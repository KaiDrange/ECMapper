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
    oscBridge(hardwareToMapperQueue, mapperToHardwareQueue, outgoingOSCQueue, logger) {
    
    hardwareService.setOSCBroadcastQueue(&outgoingOSCQueue);
    midiService.setOSCBroadcastQueue(&outgoingOSCQueue);
    
    layoutChangeHandler = std::make_unique<ecm::LayoutChangeHandler>(mapperToHardwareQueue, state.state, configLookups, [this](bool s) { suspendProcessing(s); });
    state.state.addListener(layoutChangeHandler.get());
}

ECMapperAudioProcessor::~ECMapperAudioProcessor() {
    state.state.removeListener(layoutChangeHandler.get());
}

void ECMapperAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    logger.log("prepareToPlay() called.");
    
    updateIPandPorts();
    midiService.start(state);
    hardwareService.startService();
    oscBridge.setSenderEnabled(true);
    oscBridge.setReceiverEnabled(true);
    
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
    
    // Diagnostic Heartbeat removed.
    
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
            outgoingMsg.type = ecm::osc::MessageType::Undefined;
            midiService.processMessage(msg, outgoingMsg, midiMessages);
            if (outgoingMsg.type == ecm::osc::MessageType::LED) {
                mapperToHardwareQueue.add(outgoingMsg);
            }
        }
    }
    
    midiService.reduceBreath(midiMessages);
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

void ECMapperAudioProcessor::updateIPandPorts() {
    auto ipStr = ecm::SettingsWrapper::getIP(state.state);
    juce::StringArray parts;
    parts.addTokens(ipStr, ":", "\"");
    if (parts.size() == 2) {
        oscBridge.setSenderTarget(parts[0], parts[1].getIntValue());
        oscBridge.setReceiverPort(parts[1].getIntValue() + 1);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ECMapperAudioProcessor();
}
