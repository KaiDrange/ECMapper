#pragma once
#include <atomic>
#include "MidiProtocol.h"

namespace ecm {

class Midi2Protocol : public MidiProtocol {
public:
    Midi2Protocol(uint8_t group = 0);

    void addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) override;
    void addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) override;

    void addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) override;
    void addChannelPressure(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) override;
    void addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) override;
    void addCC(juce::MidiBuffer& buffer, int channel, int noteNumber, int ccNumber, float value, int eventTime) override;

    void addProgramChange(juce::MidiBuffer& buffer, int channel, int program, int eventTime) override;
    void addAllNotesOff(juce::MidiBuffer& buffer, int channel, int eventTime) override;

    void addMidiStart(juce::MidiBuffer& buffer, int eventTime) override;
    void addMidiStop(juce::MidiBuffer& buffer, int eventTime) override;
    void addMidiContinue(juce::MidiBuffer& buffer, int eventTime) override;

    void setup(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) override;
    
    void addIdentification(juce::MidiBuffer& buffer, int eventTime) override;

    int findMidiChannelForNewNote(MidiChannelType outputType, int noteNumber) override;
    void releaseMidiChannel(MidiChannelType outputType, int noteNumber, int channel) override;

    void setRemoteSupportsPerNote(bool supports) override;

private:
    uint8_t group_ = 0;
    std::atomic<bool> remoteSupportsPerNote_ { false };
    std::unique_ptr<juce::MPEChannelAssigner> lowerChanAssigner_;
    std::unique_ptr<juce::MPEChannelAssigner> upperChanAssigner_;
};

}
