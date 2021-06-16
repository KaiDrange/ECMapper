#include "PluginProcessor.h"
#include "PluginEditor.h"

ECMapperAudioProcessor::ECMapperAudioProcessor() :
AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
//AudioProcessor(BusesProperties()),
pluginState(*this, nullptr, id_state, createParameterLayout()), osc(&oscSendQueue, &oscReceiveQueue), configLookups { ConfigLookup(DeviceType::Alpha, pluginState), ConfigLookup(DeviceType::Tau, pluginState), ConfigLookup(DeviceType::Pico, pluginState)}, midiGenerator(configLookups), layoutChangeHandler(&oscSendQueue, this, configLookups) {
    pluginState.state.addListener(&layoutChangeHandler);
    pluginState.state.addListener(this);
}

ECMapperAudioProcessor::~ECMapperAudioProcessor() {
}

const juce::String ECMapperAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool ECMapperAudioProcessor::acceptsMidi() const {
    return false;
}

bool ECMapperAudioProcessor::producesMidi() const {
    return true;
}

bool ECMapperAudioProcessor::isMidiEffect() const {
    return true;
}

double ECMapperAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int ECMapperAudioProcessor::getNumPrograms() {
    return 1;
}

int ECMapperAudioProcessor::getCurrentProgram() {
    return 0;
}

void ECMapperAudioProcessor::setCurrentProgram (int index) {
}

const juce::String ECMapperAudioProcessor::getProgramName(int index) {
    return {};
}

void ECMapperAudioProcessor::changeProgramName(int index, const juce::String& newName) {
}

void ECMapperAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
//    if (juce::JUCEApplication::isStandaloneApp()) {
//    }
    
    updateIPandPorts();
    midiGenerator.start(pluginState);
}

void ECMapperAudioProcessor::updateIPandPorts() {
    juce::StringArray ipAndPortNo;
    Utility::splitString(SettingsWrapper::getIP(pluginState.state), ":", ipAndPortNo);
    if (ipAndPortNo.size() == 2) {
        osc.senderIP = ipAndPortNo[0];
        osc.senderPort = ipAndPortNo[1].getIntValue();
        osc.receiverPort = osc.senderPort+1;
    }
}

void ECMapperAudioProcessor::releaseResources() {
    midiGenerator.stop();
    osc.disconnectSender();
    osc.disconnectReceiver();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ECMapperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    juce::ignoreUnused(layouts);
    return true;
}
#endif

void ECMapperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear();
    midiMessages.clear();
    checkConnectionChanges();
    
    static OSC::Message msg;
    static OSC::Message outgoingMsg;
    if (!layoutChangeHandler.layoutMidiRPNSent) {
        midiGenerator.createLayoutRPNs(midiMessages);
        layoutChangeHandler.layoutMidiRPNSent = true;
    }
    while (osc.receiveQueue->getMessageCount() > 0) {
        osc.receiveQueue->read(&msg);
        if (msg.type == OSC::MessageType::Device)
            layoutChangeHandler.sendLEDMsgForAllKeys(msg.device);
        else {
            outgoingMsg.type = OSC::MessageType::Undefined;
            midiGenerator.processOSCMessage(msg, outgoingMsg, midiMessages);
            if (outgoingMsg.type == OSC::MessageType::LED) {
                outgoingMsg.device = msg.device;
                outgoingMsg.course = msg.course;
                outgoingMsg.key = msg.key;
                oscSendQueue.add(&outgoingMsg);
            }
        }
    }
    
    midiGenerator.samplesSinceLastBreathMsg += buffer.getNumSamples();
    if (midiGenerator.samplesSinceLastBreathMsg > 1024)
        midiGenerator.reduceBreath(midiMessages);
}

void ECMapperAudioProcessor::processBlockBypassed(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
    if (osc.senderIsConnected)
        osc.disconnectSender();
    if (osc.receiverIsConnected)
        osc.disconnectReceiver();
}


bool ECMapperAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* ECMapperAudioProcessor::createEditor() {
    auto editor = new ECMapperAudioProcessorEditor(*this);
    editor->setResizable(true, true);
    editor->setResizeLimits(800, 600, 4096, 4096);
    this->editor = editor;

//    editor->mainComponent->addListener(&layoutChangeHandler);
    return editor;
}

void ECMapperAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    auto state = pluginState.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ECMapperAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement>xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(pluginState.state.getType())) {
            pluginState.replaceState(juce::ValueTree::fromXml(*xmlState));
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Alpha);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Tau);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Pico);

            if (editor != nullptr) {
                ((ECMapperAudioProcessorEditor*)editor)->recreateMainComponent();
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ECMapperAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout ECMapperAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout paramLayout;
//    paramLayout.add(std::make_unique<juce::AudioParameterInt>("transpose", "Transpose", -48, 48, 0));
    return paramLayout;
}

void ECMapperAudioProcessor::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    if (vTree.getType() != SettingsWrapper::id_globalSettings)
        return;
    
    if (property == SettingsWrapper::id_IP) {
        osc.disconnectSender();
        osc.disconnectReceiver();
        updateIPandPorts();
        osc.connectSender();
        osc.disconnectReceiver();
    }
    else if (property != SettingsWrapper::id_activeTab) {
        midiGenerator.stop();
        midiGenerator.start(pluginState);
    }
}

void ECMapperAudioProcessor::checkConnectionChanges() {
    if (!osc.senderIsConnected) {
        auto success = osc.connectSender();
        if (success) {
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Pico);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Tau);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Alpha);
        }
    }
    if (!osc.receiverIsConnected)
        osc.connectReceiver();
    
    if (!prevEigenCoreConnectedState && osc.eigenCoreConnected) {
        layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Pico);
        layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Tau);
        layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Alpha);
    }
    prevEigenCoreConnectedState = osc.eigenCoreConnected;
}
