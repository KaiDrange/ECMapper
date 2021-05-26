#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::ValueTree* rootState;

EigenharpMapperAudioProcessor::EigenharpMapperAudioProcessor() : AudioProcessor (BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)
), layoutChangeHandler(&oscSendQueue),
pluginState(*this, nullptr, id_state, createParameterLayout()), osc(&oscSendQueue, &oscReceiveQueue) {
    rootState = &pluginState.state;

    juce::StringArray ipAndPortNo;
    Utility::splitString(SettingsWrapper::getIP(), ":", ipAndPortNo);
    if (ipAndPortNo.size() == 2) {
        osc.connectSender(ipAndPortNo[0], ipAndPortNo[1].getIntValue());
        osc.connectReceiver(ipAndPortNo[1].getIntValue() + 1);
    }
    midiGenerator = new MidiGenerator();
    layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], DeviceType::Alpha);
    layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], DeviceType::Tau);
    layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], DeviceType::Pico);
}

EigenharpMapperAudioProcessor::~EigenharpMapperAudioProcessor() {
    osc.disconnectSender();
    osc.disconnectReceiver();
    delete midiGenerator;
}

const juce::String EigenharpMapperAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool EigenharpMapperAudioProcessor::acceptsMidi() const {
    return false;
}

bool EigenharpMapperAudioProcessor::producesMidi() const {
    return true;
}

bool EigenharpMapperAudioProcessor::isMidiEffect() const {
    return false;
}

double EigenharpMapperAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int EigenharpMapperAudioProcessor::getNumPrograms() {
    return 1;
}

int EigenharpMapperAudioProcessor::getCurrentProgram() {
    return 0;
}

void EigenharpMapperAudioProcessor::setCurrentProgram (int index) {
}

const juce::String EigenharpMapperAudioProcessor::getProgramName(int index) {
    return {};
}

void EigenharpMapperAudioProcessor::changeProgramName(int index, const juce::String& newName) {
}

void EigenharpMapperAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
}

void EigenharpMapperAudioProcessor::releaseResources() {
}

bool EigenharpMapperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    juce::ignoreUnused(layouts);
    return true;
}

void EigenharpMapperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    midiMessages.clear();
    static OSC::Message msg;
    if (!layoutChangeHandler.layoutMidiRPNSent) {
        midiGenerator->createLayoutRPNs(midiMessages);
        layoutChangeHandler.layoutMidiRPNSent = true;
    }
    while (osc.receiveQueue->getMessageCount() > 0) {
        osc.receiveQueue->read(&msg);
        midiGenerator->processOSCMessage(msg, midiMessages);
    }
}

bool EigenharpMapperAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* EigenharpMapperAudioProcessor::createEditor() {
    auto editor = new EigenharpMapperAudioProcessorEditor(*this);
    this->editor = editor;

    editor->mainComponent->addListener(&layoutChangeHandler);
    return editor;
}

void EigenharpMapperAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    auto state = pluginState.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EigenharpMapperAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement>xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(pluginState.state.getType())) {
            pluginState.replaceState(juce::ValueTree::fromXml(*xmlState));
            rootState = &pluginState.state;
            layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], DeviceType::Alpha);
            layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], DeviceType::Tau);
            layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], DeviceType::Pico);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Alpha);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Tau);
            layoutChangeHandler.sendLEDMsgForAllKeys(DeviceType::Pico);

            if (editor != nullptr) {
                ((EigenharpMapperAudioProcessorEditor*)editor)->recreateMainComponent();
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new EigenharpMapperAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout EigenharpMapperAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout paramLayout;
//    paramLayout.add(std::make_unique<juce::AudioParameterInt>("transpose", "Transpose", -48, 48, 0));
    return paramLayout;
}
