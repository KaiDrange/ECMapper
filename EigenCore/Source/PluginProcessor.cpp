#include "PluginProcessor.h"
#include "PluginEditor.h"

volatile std::atomic<bool> exitThreads;
std::atomic<bool> mapperConnected;
std::list<ConnectedDevice> connectedDevices;

EigenCoreAudioProcessor::EigenCoreAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(params[(int)ConnectionType::Pico] = new juce::AudioParameterBool("pico", "Pico connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(params[(int)ConnectionType::Tau] = new juce::AudioParameterBool("tau", "Tau connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(params[(int)ConnectionType::Alpha] = new juce::AudioParameterBool("alpha", "Alpha connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(params[(int)ConnectionType::Mapper] = new juce::AudioParameterBool("mapper", "Mapper connected", false, juce::AudioParameterBoolAttributes()));
        
    eigenCore.initialiseCore("");
    startTimer(1000);
//    if (test.confirmResources())
//    {
//    }
//    else
//        std::cout << "Couldn't find ihx files.";
    
}

EigenCoreAudioProcessor::~EigenCoreAudioProcessor()
{
    eigenCore.shutdownCore();
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

void EigenCoreAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //eigenCore.initialiseCore(juce::StringArray());
}

void EigenCoreAudioProcessor::releaseResources()
{
    //eigenCore.shutdownCore();
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
    //updateOutputParameters();
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

void EigenCoreAudioProcessor::shutdown()
{
    eigenCore.shutdownCore();
}

void EigenCoreAudioProcessor::suspended()
{
    
}

void EigenCoreAudioProcessor::resumed()
{
    
}

void EigenCoreAudioProcessor::updateOutputParameters()
{
    bool needsRepaint = false;
    bool currentConnections[4] = { false, false, false, false };
    for (auto i = begin(connectedDevices); i != end(connectedDevices); i++)
    {
        if (i->type == EHDeviceType::Pico)
            currentConnections[(int)ConnectionType::Pico] = true;
        else if (i->type == EHDeviceType::Tau)
            currentConnections[(int)ConnectionType::Tau] = true;
        else if (i->type == EHDeviceType::Alpha)
            currentConnections[(int)ConnectionType::Alpha] = true;
    }
    currentConnections[(int)ConnectionType::Mapper] = mapperConnected;

    for (int i = 0; i < 4; i++)
    {
        if (params[i]->get() != currentConnections[i])
        {
            params[i]->setValueNotifyingHost(currentConnections[i]);
            needsRepaint = true;
        }
    }
    
    if (needsRepaint)
    {
        auto editor = getActiveEditor();
        if (editor != nullptr)
            editor->repaint();
    }
}

