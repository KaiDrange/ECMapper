#include "PluginProcessor.h"
#include "PluginEditor.h"

EigenharpMapperAudioProcessor::EigenharpMapperAudioProcessor() : AudioProcessor (BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)
), layoutChangeHandler(&oscSendQueue),
pluginState(*this, nullptr, id_state, createParameterLayout()), osc(&oscSendQueue, &oscReceiveQueue)
{
    osc.connectSender("127.0.0.1", senderPort);
    osc.connectReceiver(receiverPort);
}

EigenharpMapperAudioProcessor::~EigenharpMapperAudioProcessor() {
    osc.disconnectSender();
    osc.disconnectReceiver();
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
    auto uiSettings = pluginState.state.getOrCreateChildWithName("uiSettings", nullptr);
    midiGenerator = new MidiGenerator(uiSettings);

    auto alphaLayout = uiSettings.getOrCreateChildWithName("layout1", nullptr);
    auto tauLayout = uiSettings.getOrCreateChildWithName("layout2", nullptr);
    auto picoLayout = uiSettings.getOrCreateChildWithName("layout3", nullptr);
    
    layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[0], (DeviceType)alphaLayout.getProperty("instrumentType").toString().getIntValue());
    layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[1], (DeviceType)tauLayout.getProperty("instrumentType").toString().getIntValue());
    layoutChangeHandler.setKeyConfigLookup(midiGenerator->keyConfigLookups[2], (DeviceType)picoLayout.getProperty("instrumentType").toString().getIntValue());
}

void EigenharpMapperAudioProcessor::releaseResources() {
    delete midiGenerator;
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
            for (int i = 0; i < 3; i++) {
                layoutChangeHandler.sendLEDMsgForAllKeys(pluginState.state.getChildWithName("uiSettings").getChildWithName("layout" + juce::String(i+1)));
            }
            
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
