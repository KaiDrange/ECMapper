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
#include "MidiClockOutput.h"
#include "ExpressionEmissionPolicy.h"
#include "PerformanceEventSink.h"
#include "Vst3DirectEventQueue.h"

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
        std::shared_ptr<MidiTransportSession> transportSession;
        std::shared_ptr<MidiVoiceRouter> voiceRouter;
        std::shared_ptr<ExpressionEmissionPolicy> expressionPolicy;
        RuntimeConfigSnapshot(const ConfigLookup (&source)[3],
                              std::shared_ptr<MidiProtocol> p,
                              std::shared_ptr<MidiVoiceRouter> router,
                              std::shared_ptr<ExpressionEmissionPolicy> policy)
            : configLookups { source[0], source[1], source[2] },
              protocol(std::move(p)),
              transportSession(std::dynamic_pointer_cast<MidiTransportSession>(protocol)),
              voiceRouter(std::move(router)),
              expressionPolicy(std::move(policy)) {}
    };

    MidiService(ConfigLookup (&configLookups)[3], juce::CriticalSection& stateLock);
    ~MidiService() override;
    
    void start(juce::AudioProcessorValueTreeState& pluginState, HardwareService* hs = nullptr);
    void stop();
    
    void processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, int eventTime = 0, int* presetSlotRequest = nullptr, int* transportRequest = nullptr);
    void processMessage(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer, PerformanceEventSink& sink, int eventTime = 0, int* presetSlotRequest = nullptr, int* transportRequest = nullptr);
    void handleRemotePerformanceData(osc::Message& oscMsg, juce::MidiBuffer& midiBuffer, int eventTime = 0);
    void resendLEDs(const char* devId, InstrumentType type, osc::MessageFifo* targetQueue = nullptr, bool onlyNonOff = false);
    void reduceBreath(juce::MidiBuffer& buffer, int eventTime = 0, int blockSampleCount = 0);
    void reduceBreath(juce::MidiBuffer& buffer, PerformanceEventSink& sink, int eventTime = 0, int blockSampleCount = 0);
    void createLayoutRPNs(juce::MidiBuffer& buffer);
    void queueTransposeChangeFlush(InstrumentType deviceType, Zone zone);
    void drainPendingMidiMessages(juce::MidiBuffer& buffer, int eventTime = 0);
    void scheduleMasterClock(const juce::MidiBuffer& buffer, double blockStartMs, double sampleRate) {
        clockOutput_.schedule(buffer, blockStartMs, sampleRate);
    }
    void drainDirectUMPs(juce::MidiBuffer& buffer, bool silentIfFailed = false);
    void setMidiOutput(juce::MidiOutput* output);
    void setStandaloneLegacyMidiOutputs(const std::array<juce::MidiOutput*, 3>& outputs, juce::MidiOutput* defaultOutput);
    bool isStandaloneLegacyZoneRoutingEnabled() const;
    void sendStandaloneLegacyMidiBuffers(const juce::MidiBuffer& sharedBuffer, const std::array<juce::MidiBuffer, 3>& zoneBuffers);
    void sendIdentification();
    void setRuntimeConfigSnapshot(std::unique_ptr<RuntimeConfigSnapshot> snapshot);
    void finishedBlock();
    void beginPerformanceBlock(double sampleRate);
    void endPerformanceBlock(juce::MidiBuffer& buffer, int numSamples, PerformanceEventSink* sink = nullptr);
    std::shared_ptr<MidiProtocol> getProtocol() const { return protocol_; }

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
    std::shared_ptr<MidiVoiceRouter> getVoiceRouter() { return voiceRouter_; }
    std::shared_ptr<ExpressionEmissionPolicy> getExpressionPolicy() { return expressionPolicy_; }

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
        Active = 2,
        Ignored = 3
    };
    
    struct KeyState {
        KeyStatus status = KeyStatus::Off;
        std::deque<float> ehPressureHistory;
        float ehRoll = 0.0f;
        float ehYaw = 0.0f;
        uint64_t lastTimestamp = 0;
        uint64_t noteOnTimestamp = 0;
        int midiChannel = 1;
        int noteZoneIndex = 0;
        bool hasNoteAllocation = false;
        int allocationNote = -1;
        MidiChannelType allocationOutput = MidiChannelType::Undefined;
        int stringNumber = 0;
        Zone stringZone = Zone::NoZone;
        float stringPitchOffset = 0.0f;
        LayoutWrapper::KeyId soundingKeyId;
        KeyMappingType soundingMapType = KeyMappingType::None;
        uint64_t pressSequence = 0;
        int stringTargetNote = -1;
        bool strumOwnExpression = true;
        bool strumExpressionFromKey = true;
        bool strumFretLegato = false;
        int strumTranspose = 0;
        int strumOpenNote = -1;
        float palmMutePressure = 0.0f;
        int palmMuteLastCC = -1;
        float palmMuteLastCCValue = -1.0f;
        bool palmMuteReleasePending = false;
        float palmMuteReleaseVelocity = 0.0f;
        uint64_t palmMuteReleaseSample = 0;
        uint64_t palmMuteStartBlock = 0;
        bool strumControlsNoteOff = true; // Captured at note-on.
        bool strumStartedOpen = false; // With key release disabled, sustain until muted or replaced.
        Zone strumSourceZone = Zone::NoZone;
        int strumSourceString = 0;
        LayoutWrapper::KeyId strumExpressionSource;
        uint64_t strumSourcePressSequence = 0;
        float linkedRoll = 0.0f, linkedYaw = 0.0f, linkedPressure = 0.0f;
        int messageCount = 0;
        bool isLatchOn = false;
        std::array<int, 6> strumSourceNotes { -1, -1, -1, -1, -1, -1 };
        int activeNotes[6] = { -1, -1, -1, -1, -1, -1 };
    };
    
    void releaseNoteAllocation(const LayoutWrapper::KeyId& keyId, KeyState& state, MidiVoiceRouter* voiceRouter);

    const KeyState* findStrumSource(const ConfigLookup::Key& keyLookup, int& note) const;
    void releaseUnheldStrums(int deviceIndex, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);
    void endStrum(KeyState& state, const ConfigLookup::Key& lookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);
    void updateStrummedFret(const KeyState& source, PerformanceEventSink& sink, int eventTime,
                           MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy);
    void updateLinkedStrumExpression(const KeyState& source, PerformanceEventSink& sink, int eventTime,
                                     MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy);
    void emitScheduledPalmMuteReleases(PerformanceEventSink& sink, int eventTime);
    double performanceSampleRate_ = 48000.0;
    uint64_t performanceBlockStartSample_ = 0;
    uint64_t performanceBlockNumber_ = 0;
    uint64_t nextPressSequence_ = 0;
    KeyState keyStates_[3][3][120];
    int latchTranspose_[3] = { 0, 0, 0 };
    int momentaryTranspose_[3] = { 0, 0, 0 };
    float ehBreath_[3] = { 0.0f, 0.0f, 0.0f };
    float ehStrips_[2][3] = { {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
    float relStart_ehStrips_[2][3] = { {-1.0f,-1.0f,-1.0f}, {-1.0f,-1.0f,-1.0f} };
    float currentKeyPBperChannel_[16] = {0.0f};
    float currentStripPBperChannel_[16] = {0.0f};
    
    static constexpr int PRESSURE_HISTORY_LENGTH = 6;
    static constexpr int BREATH_STABILITY_HOLD_SAMPLES = 8192;
    static constexpr float BREATH_RELEASE_PER_SAMPLE = 0.000006f;
    float breathZeroThreshold_[3] = {0.03125f, 0.03125f, 0.125f};
    float stripZeroThreshold_[3] = {0.0366f, 0.0366f, 0.12f};
    float stripSensitivity_[3] = {1.3f, 1.3f, 1.2f};
    bool invertStripDirection_[3] = {false, false, false};
    float yawSensitivity_[3] = {1.7f, 1.7f, 1.7f};
    float rollSensitivity_[3] = {1.7f, 1.7f, 1.7f};
    float pressureSensitivity_[3] = {1.7f, 1.7f, 1.7f};
    float breathSensitivity_[3] = {1.0f, 1.0f, 1.0f};
    int breathSamplesSinceUpdate_[3] = {BREATH_STABILITY_HOLD_SAMPLES, BREATH_STABILITY_HOLD_SAMPLES, BREATH_STABILITY_HOLD_SAMPLES};

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
    std::shared_ptr<MidiTransportSession> transportSession_;
    std::shared_ptr<MidiVoiceRouter> voiceRouter_;
    std::shared_ptr<ExpressionEmissionPolicy> expressionPolicy_;
    bool initialized_ = false;

    juce::universal_midi_packets::EndpointId getCorrectedEndpointId(juce::universal_midi_packets::EndpointId id, const juce::String& name);
    juce::universal_midi_packets::EndpointId getEndpointIdSafe(juce::MidiOutput* output);
    void processNoteKey(const osc::Message& oscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy);
    void processCmdKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);
    bool isPalmMuted(int deviceIndex, Zone zone) const;
    float getPalmMutePressure(int deviceIndex, Zone zone) const;
    void processJoystickKey(const osc::Message&, const ConfigLookup::Key&, KeyState*, PerformanceEventSink&, int, ExpressionEmissionPolicy*);
    void processPalmMuteKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup,
                            KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);
    void processAppCtrlKey(const osc::Message& oscMsg, osc::Message& outgoingOscMsg, const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, int* presetSlotRequest, int* transportRequest);
    
    void createNoteOn(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy);
    void createNoteOff(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, bool forceNoteOff = false, float releaseVelocity = -1.0f);
    void createNoteHold(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter, ExpressionEmissionPolicy* expressionPolicy);
    
    void createMidiMsgOn(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiVoiceRouter* voiceRouter);
    void createMidiMsgOff(const ConfigLookup::Key& keyLookup, KeyState* state, PerformanceEventSink& sink, osc::Message& outgoingOscMsg, const char* devId, int eventTime, MidiVoiceRouter* voiceRouter);
    void createAllNotesOff(PerformanceEventSink& sink, int eventTime);
    
    void addMidiValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, float pbTransportRange, int noteNo, PerformanceEventSink& sink, bool isBipolar, ExpressionCurveTarget curveTarget, int eventTime, MidiVoiceRouter* voiceRouter, int zoneIndex = -1, float pitchOffset = 0.0f);
    void addStripValueMessage(InstrumentType deviceType, int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, PerformanceEventSink& sink, bool isBipolar, int eventTime, MidiVoiceRouter* voiceRouter, int zoneIndex = -1);
    
    void createBreath(int deviceIndex, const ConfigLookup& keyLookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);
    void createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);
    void createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, const ConfigLookup& keyLookup, PerformanceEventSink& sink, int eventTime, MidiVoiceRouter* voiceRouter);

    void clearAllAppCtrlTransposes(int deviceIndex);

    float calculatePitchBendCurve(float value) const;
    float calculateNoteOnVelocity(InstrumentType deviceType, KeyState* state);
    float calculateNoteOffVelocity(InstrumentType deviceType, KeyState* state);
    float applyExpressionCurve(InstrumentType deviceType, ExpressionCurveTarget target, float value, bool isBipolar) const;
    bool isNoteOnTransitionActive(const KeyState& state, ExpressionEmissionPolicy* expressionPolicy) const;
    float applyNoteOnTransition(const KeyState& state, float measuredValue, ExpressionEmissionPolicy* expressionPolicy) const;
    int zoneIndexFromKeyId(const LayoutWrapper::KeyId& keyId) const;
    static int normalizeZoneIndex(int zoneIndex);
    void sendMidiBufferToOutput(const juce::MidiBuffer& buffer, juce::MidiOutput* output) const;
    void sendMidiBufferToDistinctOutputs(const juce::MidiBuffer& buffer, const std::array<juce::MidiOutput*, 3>& outputs, juce::MidiOutput* fallbackOutput) const;
    static float applyStripCalibration(float eigenValue, bool invertStripDirection, float zeroThreshold, float sensitivity);
    
    struct MidiNote {
        int channel;
        int noteNumber;
        int zoneIndex;
    };
    std::vector<MidiNote> playingNotes_;
    
    juce::CriticalSection pendingMessageLock_;
    juce::MidiBuffer pendingMidiBuffer_;

    std::optional<juce::universal_midi_packets::Session> umpSession_;
    juce::universal_midi_packets::Output umpOutput_;
    juce::universal_midi_packets::Input umpInput_;
    std::array<juce::universal_midi_packets::Output, 3> directUmpOutputs_;
    std::optional<juce::universal_midi_packets::LegacyVirtualOutput> virtualUmpOutput_;
    std::optional<juce::universal_midi_packets::LegacyVirtualInput> virtualUmpInputMirror_;
    std::array<juce::universal_midi_packets::VirtualEndpoint, 3> virtualEndpoints_;
    std::array<juce::universal_midi_packets::Input, 3> virtualUmpInputs_;
    std::unique_ptr<juce::InterProcessLock> virtualMidiLock_;
    juce::CriticalSection umpOutputLock_;
    std::atomic<bool> isFirstInstance_{ false };
    juce::universal_midi_packets::EndpointId lastEndpointId_;
    uint8_t umpGroup_ = 0;
    std::atomic<bool> isMidi2Mode_{ false };
    std::atomic<bool> isVirtualTarget_{ false };
    juce::String midiOutputName_ = "None";
    std::array<juce::MidiOutput*, 3> standaloneLegacyOutputs_ { nullptr, nullptr, nullptr };
    juce::MidiOutput* standaloneDefaultLegacyOutput_ = nullptr;
    juce::CriticalSection standaloneLegacyOutputLock_;
    std::array<bool, 3> standaloneClockDestinations_ {};
    void sendMasterClockMessage(uint8_t status);
    MidiClockOutput clockOutput_ { [this](uint8_t status) { sendMasterClockMessage(status); } };

    std::unique_ptr<juce::midi_ci::Device> ciDevice_;
    juce::universal_midi_packets::ToBytestreamDispatcher dispatcher_ { 4096 };
    std::atomic<bool> remoteSupportsPerNote_ { false };

    void sendCISysex (int group, juce::midi_ci::MUID destinationMUID, std::byte subID2, juce::Span<const std::byte> body, std::byte deviceID = std::byte{0x7f});
    void sendInitiateProtocolNegotiation (int group, juce::midi_ci::MUID destinationMUID, std::byte deviceID = std::byte{0x7f});
    void sendIdentityResponse (int group, std::byte deviceID);

    bool noteUsesSameOutput(const MidiNote& note, int zoneIndex) const;
    bool hasPlayingNotesOnChannel(int channel, int zoneIndex) const;
    int countPlayingNoteMatches(int channel, int noteNumber, int zoneIndex) const;
    void removeOneNoteMatch(int channel, int noteNumber, int zoneIndex);
    void appendPendingMidiMessage(const juce::MidiMessage& message, int eventTime);
    
    std::list<LayoutWrapper::KeyId> chanNotePri_[16];

    std::vector<VisualMarker> recentVisualEvents_[3][6];
    mutable juce::CriticalSection visualEventsLock_;
    void recordVisualEvent(InstrumentType deviceType, ExpressionCurveTarget target, float value, int keyId = -1);
    void logMidiExpressionMode();
};

} // namespace ecm
