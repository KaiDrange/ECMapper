#include "PluginProcessor.h"
#include "PluginEditor.h"

volatile std::atomic<bool> exitThreads;
std::atomic<bool> mapperConnected;
std::list<ConnectedDevice> connectedDevices;
static int instanceCount = 0;

EigenCoreAudioProcessor::EigenCoreAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    instanceCount++;
    if (instanceCount == 1)
        eigenCore = new EigenCore();
    addParameter(params[(int)ConnectionType::Pico] = new juce::AudioParameterBool("pico", "Pico connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(params[(int)ConnectionType::Tau] = new juce::AudioParameterBool("tau", "Tau connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(params[(int)ConnectionType::Alpha] = new juce::AudioParameterBool("alpha", "Alpha connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(params[(int)ConnectionType::Mapper] = new juce::AudioParameterBool("mapper", "Mapper connected", false, juce::AudioParameterBoolAttributes()));
    addParameter(manualShutdown = new juce::AudioParameterBool("manualShutdown", "Manual shutdown", false));

//    eigenCore.initialiseCore("");
    startTimer(1000);
}

EigenCoreAudioProcessor::~EigenCoreAudioProcessor()
{
    if (eigenCore != nullptr)
    {
        if (eigenCore->isRunning())
            eigenCore->shutdownCore();
        //delete eigenCore;
        eigenCore = nullptr;
    }
    instanceCount--;
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
    if (eigenCore != nullptr && !eigenCore->isRunning())
        eigenCore->initialiseCore("");
}

void EigenCoreAudioProcessor::releaseResources()
{
//    if (eigenCore.isRunning())
//        eigenCore.shutdownCore();
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
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        editor = new EigenCoreAudioProcessorEditor(*this);
        if(juce::TopLevelWindow::getNumTopLevelWindows() == 1)
        {
            juce::TopLevelWindow* w = juce::TopLevelWindow::getTopLevelWindow(0);
            w->setUsingNativeTitleBar(true);
        }
        return editor;
    }
    else
        return new EigenCoreAudioProcessorEditor(*this);
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
    if (eigenCore != nullptr && eigenCore->isRunning())
        eigenCore->shutdownCore();
}

void EigenCoreAudioProcessor::suspended()
{
//    if (eigenCore->isRunning())
//        eigenCore->turnOffAllLEDs();
}

void EigenCoreAudioProcessor::resumed()
{
//    if (!eigenCore.isRunning())
//        eigenCore.initialiseCore("");
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
    
    bool doShutdown = *manualShutdown;
    if (doShutdown && eigenCore != nullptr && eigenCore->isRunning())
    {
        eigenCore->shutdownCore();
    }
}

