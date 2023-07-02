#pragma once
#include <JuceHeader.h>
#include "Core/Common.h"
#include "Core/EigenCore.h"
#include "Core/FirmwareReader.h"

class EigenCoreAudioProcessor  : public juce::AudioProcessor, private juce::Timer
{
public:
    EigenCoreAudioProcessor();
    ~EigenCoreAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
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
    
    void suspended();
    void resumed();
    void shutdown();

    juce::AudioParameterBool* params[4];
    juce::AudioParameterBool* manualShutdown;
    
private:
    EigenCore *eigenCore = nullptr;
    juce::AudioProcessorEditor* editor;
    
    void timerCallback() final { updateOutputParameters(); }
    void updateOutputParameters();
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenCoreAudioProcessor)
};
