#pragma once
#include <JuceHeader.h>
#include "ConfigLookup.h"
#include "BezierCurve.h"
#include "OSCMessage.h"
#include <atomic>
#include <deque>
#include <memory>
#include <vector>
#include <list>

namespace ecm {

class HardwareService;

class MidiService {
public:
    struct RuntimeConfigSnapshot {
        std::array<ConfigLookup, 3> configLookups;
        RuntimeConfigSnapshot(const ConfigLookup (&source)[3])
            : configLookups { source[0], source[1], source[2] } {}
    };

    MidiService(ConfigLookup (&configLookups)[3], juce::CriticalSection& stateLock);
    ~MidiService();
    
    void start(juce::AudioProcessorValueTreeState& pluginState, HardwareService* hs = nullptr);
    void stop();
    
    void processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, int eventTime = 0, int* presetSlotRequest = nullptr);
    void handleRemotePerformanceData(osc::Message& oscMsg, juce::MidiBuffer& midiBuffer, int eventTime = 0);
    void resendLEDs(const char* devId, InstrumentType type, osc::MessageFifo* targetQueue = nullptr, bool onlyNonOff = false);
    void reduceBreath(juce::MidiBuffer& buffer, int eventTime = 0);
    void createLayoutRPNs(juce::MidiBuffer& buffer);
    void queueTransposeChangeFlush(InstrumentType deviceType, Zone zone);
    void drainPendingMidiMessages(juce::MidiBuffer& buffer, int eventTime = 0);
    void setRuntimeConfigSnapshot(std::unique_ptr<RuntimeConfigSnapshot> snapshot);
    void finishedBlock();

    void setOSCBroadcastQueue(osc::MessageFifo* queue) { oscBroadcastQueue_ = queue; }
    void setLocalHardwareQueue(osc::MessageFifo* queue) { localHardwareQueue_ = queue; }

    bool isInitialized() const { return initialized_; }

    struct VisualMarker {
        float value;
        juce::uint32 timestamp;
        int keyId = -1;
    };

    std::vector<VisualMarker> getVisualMarkers(InstrumentType deviceType, ExpressionCurveTarget target) const;

private:
    enum class KeyStatus {
        Off = 0,
        Pending = 1,
        Active = 2
    };
    
    struct KeyState {
        KeyStatus status = KeyStatus::Off;
        std::deque<float> ehPressureHistory;
        float ehRoll = 0.0f;
        float ehYaw = 0.0f;
        int midiChannel = 1;
        int messageCount = 0;
        bool isLatchOn = false;
        int activeNotes[4] = { -1, -1, -1, -1 };
    };
    
    KeyState keyStates_[3][3][120];
    int latchTranspose_[3] = { 0, 0, 0 };
    int momentaryTranspose_[3] = { 0, 0, 0 };
    float ehBreath_[3] = { 0.0f, 0.0f, 0.0f };
    float ehStrips_[2][3] = { {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
    float relStart_ehStrips_[2][3] = { {-1.0f,-1.0f,-1.0f}, {-1.0f,-1.0f,-1.0f} };
    int currentKeyPBperChannel_[16] = {0};
    int currentStripPBperChannel_[16] = {0};
    
    static constexpr int PRESSURE_HISTORY_LENGTH = 6;
    static constexpr float breathZeroThreshold_[3] = {0.03125f, 0.03125f, 0.125f};
    static constexpr float stripZeroThreshold_[3] = {0.0366f, 0.0366f, 0.12f};
    static constexpr float stripGain_[3] = {1.3f, 1.3f, 1.2f};
    
    std::unique_ptr<juce::MPEChannelAssigner> lowerChanAssigner_;
    std::unique_ptr<juce::MPEChannelAssigner> upperChanAssigner_;
    juce::MPEZoneLayout mpeZone_;
    
    ConfigLookup (&configLookups_)[3];
    juce::CriticalSection& stateLock_;
    
    std::atomic<RuntimeConfigSnapshot*> activeSnapshot_{ nullptr };
    
    struct DeferredSnapshot {
        std::unique_ptr<RuntimeConfigSnapshot> snapshot;
        uint64_t blockId;
    };
    std::atomic<uint64_t> currentBlockId_{ 0 };
    std::vector<DeferredSnapshot> deletionQueue_;
    juce::CriticalSection deletionQueueLock_;

    osc::MessageFifo* oscBroadcastQueue_ = nullptr;
    osc::MessageFifo* localHardwareQueue_ = nullptr;
    HardwareService* hardwareService_ = nullptr;
    juce::AudioProcessorValueTreeState* pluginState_ = nullptr;
    BezierCurve velocityCurve_ { 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.6f, 1.0f, 1.0f };
    bool initialized_ = false;

    void processNoteKey(const osc::Message& oscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime);
    void processCmdKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime);
    void processAppCtrlKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, int* presetSlotRequest);
    
    void createNoteOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime);
    void createNoteOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime);
    void createNoteHold(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime);
    
    void createMidiMsgOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime);
    void createMidiMsgOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime);
    void createAllNotesOff(juce::MidiBuffer& buffer, int eventTime = 0);
    
    void addMidiValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer& buffer, bool isBipolar, ExpressionCurveTarget curveTarget, int eventTime);
    void addStripValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer& buffer, bool isBipolar, int eventTime);
    
    void createBreath(int deviceIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime);
    void createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime);
    void createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime);

    void clearAllAppCtrlTransposes(int deviceIndex);

    float calculatePitchBendCurve(float value) const;
    juce::MPEValue calculateNoteOnVelocity(InstrumentType deviceType, KeyState* state);
    juce::MPEValue calculateNoteOffVelocity(InstrumentType deviceType, KeyState* state);
    float applyExpressionCurve(InstrumentType deviceType, ExpressionCurveTarget target, float value, bool isBipolar) const;
    
    struct MidiNote {
        int channel;
        int noteNumber;
    };
    std::vector<MidiNote> playingNotes_;
    
    struct PendingMidiMessage {
        juce::MidiMessage message;
        int eventTime = 0;
    };
    juce::CriticalSection pendingMessageLock_;
    std::vector<PendingMidiMessage> pendingMidiMessages_;

    int countPlayingNoteMatches(int channel, int noteNumber) const;
    void removeOneNoteMatch(int channel, int noteNumber);
    void appendPendingMidiMessage(const juce::MidiMessage& message, int eventTime);
    
    std::list<LayoutWrapper::KeyId> chanNotePri_[16];

    std::vector<VisualMarker> recentVisualEvents_[3][6];
    mutable juce::CriticalSection visualEventsLock_;
    void recordVisualEvent(InstrumentType deviceType, ExpressionCurveTarget target, float value, int keyId = -1);
};

} // namespace ecm
