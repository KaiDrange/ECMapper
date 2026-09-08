#pragma once
#include <atomic>
#include "MidiProtocol.h"

namespace ecm {

class Midi2Protocol : public MidiProtocol,
                      public MidiTransportSession {
public:
    Midi2Protocol(uint8_t group = 0);

    void renderEvent(juce::MidiBuffer& buffer, const PerformanceEvent& event) override;
    void setupTransport(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) override;
    void addIdentification(juce::MidiBuffer& buffer, int eventTime) override;

private:
    void addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime);
    void addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime);
    void addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime);
    void addChannelPressure(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime);
    void addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime);
    void addCC(juce::MidiBuffer& buffer, int channel, int noteNumber, int ccNumber, float value, int eventTime);
    void addProgramChange(juce::MidiBuffer& buffer, int channel, int program, int eventTime);
    void addAllNotesOff(juce::MidiBuffer& buffer, int channel, int eventTime);
    void addMidiStart(juce::MidiBuffer& buffer, int eventTime);
    void addMidiStop(juce::MidiBuffer& buffer, int eventTime);
    void addMidiContinue(juce::MidiBuffer& buffer, int eventTime);

    uint8_t group_ = 0;
};

}
