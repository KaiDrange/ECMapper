#pragma once

#include <JuceHeader.h>
#include "UI/MainComponent.h"
#include "Data/OSCCommunication.h"
#include "Data/LayoutChangeHandler.h"
#include "Models/Enums.h"
#include "Data/MidiGenerator.h"
#include "Models/SettingsWrapper.h"
#include "UI/Utility.h"
#include "Data/Logger.h"

class ECMapperAudioProcessor  : public juce::AudioProcessor, public juce::ValueTree::Listener
{
public:
    ECMapperAudioProcessor();
    ~ECMapperAudioProcessor() override;

    ECMLogger logger;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override;
    
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::Identifier id_state = "pluginState";
    juce::AudioProcessorValueTreeState pluginState;
private:
    void updateIPandPorts();
    void checkConnectionChanges();
    void refreshLights();
    
    OSCCommunication osc;
    OSC::OSCMessageFifo oscSendQueue;
    OSC::OSCMessageFifo oscReceiveQueue;
    ConfigLookup configLookups[3];
    MidiGenerator midiGenerator;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorEditor *editor = nullptr;
    LayoutChangeHandler layoutChangeHandler;
    bool prevEigenCoreConnectedState = false;
    bool isBypassed = false;
    
    
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessor)
};
