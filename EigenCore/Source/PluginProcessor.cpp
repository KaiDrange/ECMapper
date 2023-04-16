#include "PluginProcessor.h"
#include "PluginEditor.h"

EigenCoreAudioProcessor::EigenCoreAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

EigenCoreAudioProcessor::~EigenCoreAudioProcessor()
{
}

const juce::String EigenCoreAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EigenCoreAudioProcessor::acceptsMidi() const
{
    return false;
}

bool EigenCoreAudioProcessor::producesMidi() const
{
    return false;
}

bool EigenCoreAudioProcessor::isMidiEffect() const
{
    return false;
}

double EigenCoreAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EigenCoreAudioProcessor::getNumPrograms()
{
    return 1;
}

int EigenCoreAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EigenCoreAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EigenCoreAudioProcessor::getProgramName (int index)
{
    return {};
}

void EigenCoreAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EigenCoreAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void EigenCoreAudioProcessor::releaseResources()
{
}

bool EigenCoreAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void EigenCoreAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (juce::JUCEApplicationBase::isStandaloneApp())
        buffer.clear();
}

bool EigenCoreAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EigenCoreAudioProcessor::createEditor()
{
    return new EigenCoreAudioProcessorEditor (*this);
}

void EigenCoreAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EigenCoreAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EigenCoreAudioProcessor();
}
