#pragma once

#include <JuceHeader.h>
#include "UI/MainComponent.h"
#include "Data/OSCCommunication.h"
#include "Data/LayoutChangeHandler.h"
#include "Models/Enums.h"
#include "Data/MidiGenerator.h"

class EigenharpMapperAudioProcessor  : public juce::AudioProcessor
{
public:
    EigenharpMapperAudioProcessor();
    ~EigenharpMapperAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    LayoutChangeHandler layoutChangeHandler;
    juce::Identifier id_state = "pluginState";
    juce::AudioProcessorValueTreeState pluginState;
private:
    OSCCommunication osc;
    const int senderPort = 7000;
    const int receiverPort = 7001;
    OSC::OSCMessageFifo oscSendQueue;
    OSC::OSCMessageFifo oscReceiveQueue;
    MidiGenerator *midiGenerator;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorEditor *editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpMapperAudioProcessor)
};
