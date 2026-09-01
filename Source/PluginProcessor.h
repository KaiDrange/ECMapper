#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Core/HardwareService.h"
#include "Core/MidiService.h"
#include "Core/OSCBridge.h"
#include "Core/ConfigLookup.h"
#include "Core/LayoutChangeHandler.h"
#include "Core/Logger.h"
#include <array>
#include <map>
#include <vector>

class ECMapperAudioProcessor : public juce::AudioProcessor,
                               public ecm::HardwareService::Listener
{
public:
    static constexpr int numPresetSlots = 32;
    static inline const juce::String presetSlotParameterId { "presetSlot" };

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

    int getCurrentPresetSlot() const noexcept { return currentPresetSlot_; }
    juce::String getCurrentPresetName() const;
    juce::String getCurrentPresetDisplayName() const;
    juce::String getPresetSlotDisplayName(int slot) const;
    bool hasPresetSlot(int slot) const;
    bool hasUnsavedChanges() const;
    bool savePresetSlot(int slot, const juce::String& name);
    bool deletePresetSlot(int slot);
    bool loadPresetSlot(int slot);
    void loadStandalonePresetBank();
    void saveStandalonePresetBank() const;

    // HardwareService::Listener overrides
    void deviceListChanged() override;
    void deviceNeedsLEDSync(const std::string& devId, ecm::InstrumentType type, bool isRequest) override;

    juce::AudioProcessorValueTreeState state;
    ecm::HardwareService& getHardwareService() { return hardwareService; }
    
    void setDeviceManager(juce::AudioDeviceManager* manager) { deviceManager = manager; }
    juce::AudioDeviceManager* getDeviceManager() { return deviceManager; }

    void queueKeyboardSelectionMessage(const juce::MidiMessage& message);
    void drainKeyboardSelectionMessages(std::vector<juce::MidiMessage>& messages);
    void clearKeyboardSelectionMessages();

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
    double lastBlockEndUs = 0.0;
    double localClockOffset = 0.0;
    std::map<juce::String, double> remoteClockOffsets;
    std::array<int, 9> transposeCache_ {};
    std::array<int, 9> enableCache_ {};
    bool transposeCacheInitialised_ = false;
    bool enableCacheInitialised_ = false;
    std::vector<juce::MidiMessage> keyboardSelectionMessages_;
    juce::CriticalSection keyboardSelectionLock_;
    juce::ValueTree presetBankState_ { "ECMapperPresetBank" };
    juce::ValueTree factoryDefaultState_;
    juce::ValueTree currentPresetState_;
    juce::AudioParameterChoice* presetSlotParameter_ = nullptr;
    int currentPresetSlot_ = 1;
    juce::String currentPresetName_ { "Default" };
    bool ignorePresetParameterUpdate_ = false;
    int lastPresetParameterIndex_ = 0;

    void updateGlobalSettings();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void syncZoneParameters(juce::MidiBuffer& midiMessages);
    static int transposeIndex(ecm::InstrumentType deviceType, ecm::Zone zone);
    juce::ValueTree getPresetNode(int slot) const;
    juce::ValueTree getPresetSnapshot(int slot) const;
    void applyPresetState(const juce::ValueTree& snapshot);
    void setCurrentPresetSelection(int slot, const juce::String& name);
    void resetPresetBankToDefault();
    juce::File getStandalonePresetBankFile() const;
    static juce::ValueTree makeComparableState(juce::ValueTree state);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessor)
};
