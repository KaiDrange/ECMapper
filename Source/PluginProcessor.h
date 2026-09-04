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
#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

class ECMapperAudioProcessor : public juce::AudioProcessor,
                               public ecm::HardwareService::Listener,
                               private juce::AsyncUpdater,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int numPresetSlots = 32;
    static inline const juce::String presetSlotParameterId { "presetSlot" };

    ECMapperAudioProcessor();
    ~ECMapperAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    int getCurrentPresetSlot() const noexcept { return currentPresetSlot_.load(); }
    juce::String getCurrentPresetName() const;
    juce::String getCurrentPresetDisplayName() const;
    juce::String getPresetSlotDisplayName(int slot) const;
    bool hasPresetSlot(int slot) const;
    bool savePresetSlot(int slot, const juce::String& name);
    bool deletePresetSlot(int slot);
    bool loadPresetSlot(int slot);
    void loadStandalonePresetBank();
    void saveStandalonePresetBank() const;

    // HardwareService::Listener overrides
    void deviceListChanged() override;
    void deviceNeedsLEDSync(const std::string& devId, ecm::InstrumentType type, bool isRequest) override;
    void handleAsyncUpdate() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState state;
    ecm::HardwareService& getHardwareService() { return hardwareService; }
    ecm::MidiService& getMidiService() { return midiService; }

    void setDeviceManager(juce::AudioDeviceManager* manager) { deviceManager = manager; }
    juce::AudioDeviceManager* getDeviceManager() const { return deviceManager; }

    void queueKeyboardSelectionMessage(const juce::MidiMessage& message);
    void drainKeyboardSelectionMessages(std::vector<juce::MidiMessage>& messages);
    void clearKeyboardSelectionMessages();
    juce::ValueTree getPresetNode(int slot) const;
    juce::AudioProcessorEditor* createUI() { return createEditor(); }

protected:
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

private:
    juce::AudioProcessorEditor* createEditor() override;

    ecm::Logger logger { false, true };
    ecm::osc::MessageFifo hardwareToMapperQueue;
    ecm::osc::MessageFifo mapperToHardwareQueue;
    ecm::osc::MessageFifo outgoingOSCQueue;
    mutable juce::CriticalSection presetStateLock_;

    ecm::ConfigLookup configLookups[3];
    ecm::HardwareService hardwareService;
    ecm::MidiService midiService;
    ecm::OSCBridge oscBridge;
    
    std::unique_ptr<ecm::LayoutChangeHandler> layoutChangeHandler;

    juce::AudioDeviceManager* deviceManager = nullptr;
    double lastBlockEndUs = 0.0;
    double localClockOffset = 0.0;
    std::map<std::string, double, std::less<>> remoteClockOffsets;
    std::array<int, 9> transposeCache_ {};
    std::array<int, 9> enableCache_ {};
    bool transposeCacheInitialised_ = false;
    bool enableCacheInitialised_ = false;
    std::vector<juce::MidiMessage> keyboardSelectionMessages_;
    juce::CriticalSection keyboardSelectionLock_;
    juce::ValueTree presetBankState_ { "ECMapperPresetBank" };
    juce::AudioParameterChoice* presetSlotParameter_ = nullptr;
    std::atomic<int> currentPresetSlot_ { 1 };
    juce::String currentPresetName_ { "Init" };
    std::atomic<bool> ignorePresetParameterUpdate_ { false };
    std::atomic<int> lastPresetParameterIndex_ { 0 };
    bool presetBatchInProgress_ = false;
    std::atomic<int> slotToLoadAsync_ { -1 };
    std::atomic<bool> runtimeConfigRefreshRequested_ { false };

    void updateGlobalSettings();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bool applyZoneControlMessages(const juce::MidiBuffer& midiMessages) const;
    void refreshZoneRuntimeStateFromParameters();
    void requestRuntimeConfigRefresh();
    void registerZoneParameterListeners();
    void unregisterZoneParameterListeners();
    static bool isZoneRuntimeParameter(const juce::String& parameterID);
    static int transposeIndex(ecm::InstrumentType deviceType, ecm::Zone zone);
    juce::ValueTree getPresetSnapshot(int slot) const;
    void applyPresetState(const juce::ValueTree& snapshot);
    void refreshDerivedStateAfterPresetChange();
    void setCurrentPresetSelection(int slot, const juce::String& name);
    void ensureInitPresetExists();
    static juce::File getStandalonePresetBankFile();
    static juce::ValueTree makeComparableState(juce::ValueTree state);

    struct BlockTiming {
        int numSamples = 0;
        double sampleRate = 0.0;
        double blockDurationUs = 0.0;
        double nowUs = 0.0;
        double blockStartUs = 0.0;
    };

    BlockTiming calculateBlockTiming(const juce::AudioBuffer<float>& audioBuffer);
    void prepareMidiMessagesForBlock(juce::MidiBuffer& midiMessages);
    void processHardwareMessagesForBlock(const BlockTiming& timing, juce::MidiBuffer& midiMessages, int& slotToLoad);
    void handleHardwareMessage(const ecm::osc::Message& msg, const BlockTiming& timing, juce::MidiBuffer& midiMessages, int& slotToLoad);
    void dispatchPresetSlotLoad(int slotToLoad);
    void collectPresetSlotLoadRequests(const juce::MidiBuffer& midiMessages, int& slotToLoad);
    static void queuePresetSlotLoad(int slot, int& slotToLoad);
    void publishRuntimeConfigSnapshot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessor)
};
