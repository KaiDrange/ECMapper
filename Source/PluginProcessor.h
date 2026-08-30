#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/HardwareService.h"
#include "Core/MidiService.h"
#include "Core/OSCBridge.h"
#include "Core/ConfigLookup.h"
#include "Core/LayoutChangeHandler.h"
#include "Core/Logger.h"

class ECMapperAudioProcessor : public juce::AudioProcessor
{
public:
    ECMapperAudioProcessor();
    ~ECMapperAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ECMapper"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;
    ecm::HardwareService& getHardwareService() { return hardwareService; }
    
    void setDeviceManager(juce::AudioDeviceManager* manager) { deviceManager = manager; }
    juce::AudioDeviceManager* getDeviceManager() { return deviceManager; }

private:
    ecm::Logger logger { false, true };
    ecm::osc::MessageFifo hardwareToMapperQueue;
    ecm::osc::MessageFifo mapperToHardwareQueue;
    ecm::osc::MessageFifo outgoingOSCQueue;

    ecm::ConfigLookup configLookups[3];
    ecm::HardwareService hardwareService;
    ecm::MidiService midiService;
    ecm::OSCBridge oscBridge;
    
    std::unique_ptr<ecm::LayoutChangeHandler> layoutChangeHandler;

    juce::AudioDeviceManager* deviceManager = nullptr;

    void updateIPandPorts();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessor)
};
