#include "PluginProcessor.h"
#include "PluginEditor.h"

EigenharpMapperAudioProcessor::EigenharpMapperAudioProcessor() : AudioProcessor (BusesProperties()), lpLookup(&oscSendQueue), osc(&oscSendQueue, &oscReceiveQueue) {
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
    return true;
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

void EigenharpMapperAudioProcessor::releaseResources()
{
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
    
//    auto midiMsg = juce::MidiMessage::controllerEvent(1, 3, 99);
//    midiMessages.addEvent(midiMsg, 0);
    
}

bool EigenharpMapperAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* EigenharpMapperAudioProcessor::createEditor() {
    auto editor = new EigenharpMapperAudioProcessorEditor(*this);
    auto layout = editor->getActiveLayout();
    lpLookup.setActiveLayout(layout);
    layout->addListener(&lpLookup);
    return editor;
}

void EigenharpMapperAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EigenharpMapperAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new EigenharpMapperAudioProcessor();
}
