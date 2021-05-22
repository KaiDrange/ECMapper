#include "PluginProcessor.h"
#include "PluginEditor.h"

EigenharpMapperAudioProcessor::EigenharpMapperAudioProcessor() : AudioProcessor (BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)
), layoutChangeHandler(&oscSendQueue), osc(&oscSendQueue, &oscReceiveQueue),
pluginState(*this, nullptr, juce::Identifier("pluginState"), createParameterLayout())
{
    osc.connectSender("127.0.0.1", senderPort);
    osc.connectReceiver(receiverPort);
    
    auto editor = new EigenharpMapperAudioProcessorEditor(*this);
    this->editor = editor;
    
    auto alphaLayout = editor->getLayout(DeviceType::Alpha);
    auto tauLayout = editor->getLayout(DeviceType::Tau);
    auto picoLayout = editor->getLayout(DeviceType::Pico);
    midiGenerator.keyConfigLookups[0].setLayout(alphaLayout);
    midiGenerator.keyConfigLookups[1].setLayout(tauLayout);
    midiGenerator.keyConfigLookups[2].setLayout(picoLayout);
    layoutChangeHandler.setKeyConfigLookup(&midiGenerator.keyConfigLookups[0], alphaLayout->getDeviceType());
    layoutChangeHandler.setKeyConfigLookup(&midiGenerator.keyConfigLookups[1], tauLayout->getDeviceType());
    layoutChangeHandler.setKeyConfigLookup(&midiGenerator.keyConfigLookups[2], picoLayout->getDeviceType());
//    alphaLayout->addListener(&layoutChangeHandler);
//    tauLayout->addListener(&layoutChangeHandler);
//    picoLayout->addListener(&layoutChangeHandler);
    
    pluginState.state.addChild(editor->mainComponent.uiSettings, 0, nullptr);
    editor->mainComponent.addListener(&layoutChangeHandler);

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

const juce::String EigenharpMapperAudioProcessor::getProgramName (int index) {
    return {};
}

void EigenharpMapperAudioProcessor::changeProgramName (int index, const juce::String& newName) {
}

void EigenharpMapperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void EigenharpMapperAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool EigenharpMapperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
    juce::ignoreUnused(layouts);
    return true;
}

void EigenharpMapperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
//        auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }
        
    static OSC::Message msg;
    if (!layoutChangeHandler.layoutMidiRPNSent) {
        midiGenerator.createLayoutRPNs(midiMessages);
        layoutChangeHandler.layoutMidiRPNSent = true;
    }
    while (osc.receiveQueue->getMessageCount() > 0) {
        osc.receiveQueue->read(&msg);
        midiGenerator.processOSCMessage(msg, midiMessages);
    }
}

bool EigenharpMapperAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* EigenharpMapperAudioProcessor::createEditor() {
    
    return editor;
}

void EigenharpMapperAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    auto state = pluginState.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EigenharpMapperAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(pluginState.state.getType())) {
            auto newTreeState = juce::ValueTree::fromXml (*xmlState);
            copyTreePropertiesRecursive(newTreeState, pluginState.state);
            if (editor != nullptr)
                editor->repaint();
        }
    }
}

void EigenharpMapperAudioProcessor::copyTreePropertiesRecursive(const juce::ValueTree source, juce::ValueTree dest) {
    dest.copyPropertiesFrom(source, nullptr);
    for (int i = 0; i < dest.getNumChildren(); i++)
        copyTreePropertiesRecursive(source.getChild(i), dest.getChild(i));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new EigenharpMapperAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout EigenharpMapperAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout paramLayout;
    paramLayout.add(std::make_unique<juce::AudioParameterInt>("transpose", "Transpose", -48, 48, 0));
    return paramLayout;
}
