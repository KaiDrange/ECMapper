#pragma once
#include <JuceHeader.h>
#include "ConfigLookup.h"
#include "OSCMessage.h"
#include "BezierCurve.h"
#include <deque>
#include <vector>
#include <list>

namespace ecm {

class MidiService {
public:
    MidiService(ConfigLookup (&configLookups)[3]);
    ~MidiService();
    
    void start(juce::AudioProcessorValueTreeState& pluginState);
    void stop();
    
    void processMessage(osc::Message& oscMsg, osc::Message& outgoingOscMsg, juce::MidiBuffer& midiBuffer);
    void reduceBreath(juce::MidiBuffer& buffer);
    void createLayoutRPNs(juce::MidiBuffer& buffer);

    void setOSCBroadcastQueue(osc::MessageFifo* queue) { oscBroadcastQueue_ = queue; }

    bool isInitialized() const { return initialized_; }

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
    };
    
    KeyState keyStates_[3][3][120];
    float ehBreath_[3] = { 0.0f, 0.0f, 0.0f };
    float ehStrips_[2][3] = { {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
    float relStart_ehStrips_[2][3] = { {-1.0f,-1.0f,-1.0f}, {-1.0f,-1.0f,-1.0f} };
    int currentKeyPBperChannel_[16] = {0};
    int currentStripPBperChannel_[16] = {0};
    
    static constexpr int PRESSURE_HISTORY_LENGTH = 6;
    static constexpr float breathZeroThreshold_[3] = {0.06f, 0.06f, 0.2f};
    
    std::unique_ptr<juce::MPEChannelAssigner> lowerChanAssigner_;
    std::unique_ptr<juce::MPEChannelAssigner> upperChanAssigner_;
    juce::MPEZoneLayout mpeZone_;
    
    ConfigLookup (&configLookups_)[3];
    osc::MessageFifo* oscBroadcastQueue_ = nullptr;
    BezierCurve velocityCurve_ { 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.6f, 1.0f, 1.0f };
    bool initialized_ = false;

    void processNoteKey(osc::Message& oscMsg, ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer);
    void processCmdKey(osc::Message& oscMsg, osc::Message& outgoingOscMsg, ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer);
    
    void createNoteOn(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer);
    void createNoteOff(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer);
    void createNoteHold(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer);
    
    void createMidiMsgOn(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg);
    void createMidiMsgOff(ConfigLookup::Key& keyLookup, KeyState* state, juce::MidiBuffer& buffer, osc::Message& outgoingOscMsg);
    void createAllNotesOff(juce::MidiBuffer& buffer);
    
    void addMidiValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, float pbRange, int noteNo, juce::MidiBuffer& buffer, bool isBipolar);
    void addStripValueMessage(int channel, float ehValue, ZoneWrapper::MidiValue midiValue, juce::MidiBuffer& buffer, bool isBipolar);
    
    void createBreath(int deviceIndex, ConfigLookup& keyLookup, juce::MidiBuffer& buffer);
    void createStripAbsolute(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup& keyLookup, juce::MidiBuffer& buffer);
    void createStripRelative(int deviceIndex, int stripIndex, int zoneIndex, ConfigLookup& keyLookup, juce::MidiBuffer& buffer);

    float calculatePitchBendCurve(float value) const;
    juce::MPEValue calculateNoteOnVelocity(KeyState* state);
    juce::MPEValue calculateNoteOffVelocity(KeyState* state);
    
    struct MidiNote {
        int channel;
        int noteNumber;
    };
    std::vector<MidiNote> playingNotes_;
    
    int countPlayingNoteMatches(int channel, int noteNumber) const;
    void removeOneNoteMatch(int channel, int noteNumber);
    
    std::list<LayoutWrapper::KeyId> chanNotePri_[16];
};

} // namespace ecm
