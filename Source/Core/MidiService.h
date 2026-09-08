#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_midi_ci/juce_midi_ci.h>
#include "ConfigLookup.h"
#include "BezierCurve.h"
#include "OSCMessage.h"
#include <atomic>
#include <deque>
#include <memory>
#include <vector>
#include <list>
#include "MidiProtocol.h"

namespace ecm {

class HardwareService;

class MidiService : public juce::ValueTree::Listener,
                    public juce::universal_midi_packets::EndpointsListener,
                    public juce::universal_midi_packets::Consumer,
                    public juce::midi_ci::DeviceMessageHandler,
                    public juce::midi_ci::ProfileDelegate,
                    public juce::midi_ci::PropertyDelegate,
                    public juce::midi_ci::DeviceListener {
public:
    struct RuntimeConfigSnapshot {
        std::array<ConfigLookup, 3> configLookups;
        std::shared_ptr<MidiProtocol> protocol;
        RuntimeConfigSnapshot(const ConfigLookup (&source)[3], std::shared_ptr<MidiProtocol> p)
            : configLookups { source[0], source[1], source[2] }, protocol(std::move(p)) {}
    };

    MidiService(ConfigLookup (&configLookups)[3], juce::CriticalSection& stateLock);
    ~MidiService() override;
    
    void start(juce::AudioProcessorValueTreeState& pluginState, HardwareService* hs = nullptr);
    void stop();
    
    void processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, int eventTime = 0, int* presetSlotRequest = nullptr);
    void handleRemotePerformanceData(osc::Message& oscMsg, juce::MidiBuffer& midiBuffer, int eventTime = 0);
    void resendLEDs(const char* devId, InstrumentType type, osc::MessageFifo* targetQueue = nullptr, bool onlyNonOff = false);
    void reduceBreath(juce::MidiBuffer& buffer, int eventTime = 0);
    void createLayoutRPNs(juce::MidiBuffer& buffer);
    void queueTransposeChangeFlush(InstrumentType deviceType, Zone zone);
    void drainPendingMidiMessages(juce::MidiBuffer& buffer, int eventTime = 0);
    void drainDirectUMPs(juce::MidiBuffer& buffer, bool silentIfFailed = false);
    void setMidiOutput(juce::MidiOutput* output);
    void sendIdentification();
    void setRuntimeConfigSnapshot(std::unique_ptr<RuntimeConfigSnapshot> snapshot);
    void finishedBlock();

    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property) override;
    void valueTreeRedirected(juce::ValueTree& treeWhichHasBeenChanged) override;
    void endpointsChanged() override;
    void consume (juce::universal_midi_packets::Iterator b, juce::universal_midi_packets::Iterator e, double time) override;

    // DeviceMessageHandler
    void processMessage (juce::universal_midi_packets::BytesOnGroup msg) override;

    // ProfileDelegate
    void profileEnablementRequested (juce::midi_ci::MUID x, juce::midi_ci::ProfileAtAddress profileAtAddress, int numChannels, bool enabled) override;

    // PropertyDelegate
    juce::midi_ci::PropertyReplyData propertyGetDataRequested (juce::midi_ci::MUID, const juce::midi_ci::PropertyRequestHeader&) override;
    juce::midi_ci::PropertyReplyHeader propertySetDataRequested (juce::midi_ci::MUID, const juce::midi_ci::PropertyRequestData&) override;
    bool subscriptionStartRequested (juce::midi_ci::MUID, const juce::midi_ci::PropertySubscriptionHeader&) override;
    void subscriptionDidStart (juce::midi_ci::MUID, const juce::String& subId, const juce::midi_ci::PropertySubscriptionHeader&) override;
    void subscriptionWillEnd (juce::midi_ci::MUID, const juce::midi_ci::Subscription& sub) override;

    // DeviceListener
    void deviceAdded (juce::midi_ci::MUID x) override;
    void deviceRemoved (juce::midi_ci::MUID x) override;
    void profileStateReceived (juce::midi_ci::MUID x, juce::midi_ci::ChannelInGroup destination) override;
    void profileEnablementChanged (juce::midi_ci::MUID x, juce::midi_ci::ChannelInGroup destination, juce::midi_ci::Profile profile, int numChannels) override;

    void updateVirtualOutput();
    bool isVirtualOutputActive() const;
    bool isUsingUMPPath() const;

    void setOSCBroadcastQueue(osc::MessageFifo* queue) { oscBroadcastQueue_ = queue; }
    void setLocalHardwareQueue(osc::MessageFifo* queue) { localHardwareQueue_ = queue; }
    
    JUCE_DECLARE_WEAK_REFERENCEABLE(MidiService)

    bool isInitialized() const { return initialized_; }
    
    std::shared_ptr<MidiProtocol> getProtocol() { return protocol_; }

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
    float currentKeyPBperChannel_[16] = {0.0f};
    float currentStripPBperChannel_[16] = {0.0f};
    
    static constexpr int PRESSURE_HISTORY_LENGTH = 6;
    float breathZeroThreshold_[3] = {0.03125f, 0.03125f, 0.125f};
    float stripZeroThreshold_[3] = {0.0366f, 0.0366f, 0.12f};
    float stripSensitivity_[3] = {1.3f, 1.3f, 1.2f};
    float yawSensitivity_[3] = {1.7f, 1.7f, 1.7f};
    float rollSensitivity_[3] = {1.7f, 1.7f, 1.7f};
    float pressureSensitivity_[3] = {1.7f, 1.7f, 1.7f};
    float breathSensitivity_[3] = {1.0f, 1.0f, 1.0f};

    void updateCalibration();
    
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
    std::shared_ptr<MidiProtocol> protocol_;
    bool initialized_ = false;

    juce::universal_midi_packets::EndpointId getCorrectedEndpointId(juce::universal_midi_packets::EndpointId id, const juce::String& name);
    juce::universal_midi_packets::EndpointId getEndpointIdSafe(juce::MidiOutput* output);
    void processNoteKey(const osc::Message& oscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    void processCmdKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    void processAppCtrlKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, int* presetSlotRequest);
    
    void createNoteOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    void createNoteOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    void createNoteHold(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    
    void createMidiMsgOn(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiProtocol* protocol);
    void createMidiMsgOff(const ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiProtocol* protocol);
    void createAllNotesOff(juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    
    void addMidiValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer& buffer, bool isBipolar, ExpressionCurveTarget curveTarget, int eventTime, MidiProtocol* protocol);
    void addStripValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, juce::MidiBuffer& buffer, bool isBipolar, int eventTime, MidiProtocol* protocol);
    
    void createBreath(int deviceIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    void createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);
    void createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, juce::MidiBuffer& buffer, int eventTime, MidiProtocol* protocol);

    void clearAllAppCtrlTransposes(int deviceIndex);

    float calculatePitchBendCurve(float value) const;
    float calculateNoteOnVelocity(InstrumentType deviceType, KeyState* state);
    float calculateNoteOffVelocity(InstrumentType deviceType, KeyState* state);
    float applyExpressionCurve(InstrumentType deviceType, ExpressionCurveTarget target, float value, bool isBipolar) const;
    
    struct MidiNote {
        int channel;
        int noteNumber;
    };
    std::vector<MidiNote> playingNotes_;
    
    juce::CriticalSection pendingMessageLock_;
    juce::MidiBuffer pendingMidiBuffer_;

    std::optional<juce::universal_midi_packets::Session> umpSession_;
    juce::universal_midi_packets::Output umpOutput_;
    juce::universal_midi_packets::Input umpInput_;
    juce::universal_midi_packets::Output directUmpOutput_;
    std::optional<juce::universal_midi_packets::LegacyVirtualOutput> virtualUmpOutput_;
    std::optional<juce::universal_midi_packets::LegacyVirtualInput> virtualUmpInputMirror_;
    juce::universal_midi_packets::VirtualEndpoint virtualEndpoint_;
    juce::universal_midi_packets::Input virtualUmpInput_;
    std::unique_ptr<juce::InterProcessLock> virtualMidiLock_;
    juce::CriticalSection umpOutputLock_;
    std::atomic<bool> isFirstInstance_{ false };
    juce::universal_midi_packets::EndpointId lastEndpointId_;
    uint8_t umpGroup_ = 0;
    std::atomic<bool> isMidi2Mode_{ false };
    std::atomic<bool> isVirtualTarget_{ false };
    juce::String midiOutputName_ = "None";

    std::unique_ptr<juce::midi_ci::Device> ciDevice_;
    juce::universal_midi_packets::ToBytestreamDispatcher dispatcher_ { 4096 };
    std::atomic<bool> remoteSupportsPerNote_ { false };

    void sendCISysex (int group, juce::midi_ci::MUID destinationMUID, std::byte subID2, juce::Span<const std::byte> body, std::byte deviceID = std::byte{0x7f});
    void sendInitiateProtocolNegotiation (int group, juce::midi_ci::MUID destinationMUID, std::byte deviceID = std::byte{0x7f});
    void sendIdentityResponse (int group, std::byte deviceID);

    int countPlayingNoteMatches(int channel, int noteNumber) const;
    void removeOneNoteMatch(int channel, int noteNumber);
    void appendPendingMidiMessage(const juce::MidiMessage& message, int eventTime);
    
    std::list<LayoutWrapper::KeyId> chanNotePri_[16];

    std::vector<VisualMarker> recentVisualEvents_[3][6];
    mutable juce::CriticalSection visualEventsLock_;
    void recordVisualEvent(InstrumentType deviceType, ExpressionCurveTarget target, float value, int keyId = -1);
    void logMidiExpressionMode();
};

} // namespace ecm
