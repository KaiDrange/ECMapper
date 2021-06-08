#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::ValueTree* rootState;

ECMapperAudioProcessor::ECMapperAudioProcessor() : AudioProcessor (BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
pluginState(*this, nullptr, id_state, createParameterLayout()), osc(&oscSendQueue, &oscReceiveQueue), configLookups { ConfigLookup(DeviceType::Alpha), ConfigLookup(DeviceType::Tau), ConfigLookup(DeviceType::Pico)}, midiGenerator(configLookups), layoutChangeHandler(&oscSendQueue, this, configLookups) {
    rootState = &pluginState.state;

    juce::StringArray ipAndPortNo;
    Utility::splitString(SettingsWrapper::getIP(), ":", ipAndPortNo);
    if (ipAndPortNo.size() == 2) {
        osc.connectSender(ipAndPortNo[0], ipAndPortNo[1].getIntValue());
        osc.connectReceiver(ipAndPortNo[1].getIntValue() + 1);
    }
//    midiGenerator = new MidiGenerator(configLookups);
    pluginState.state.addListener(&layoutChangeHandler);
}

ECMapperAudioProcessor::~ECMapperAudioProcessor() {
    osc.disconnectSender();
    osc.disconnectReceiver();
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
    if (juce::JUCEApplication::isStandaloneApp()) {
    }
    
    midiGenerator.start();
}

void ECMapperAudioProcessor::releaseResources() {
    midiGenerator.stop();
}

bool ECMapperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    juce::ignoreUnused(layouts);
    return true;
}

void ECMapperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    midiMessages.clear();
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

bool ECMapperAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* ECMapperAudioProcessor::createEditor() {
    auto editor = new ECMapperAudioProcessorEditor(*this);
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
            rootState = &pluginState.state;
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

