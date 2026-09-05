#pragma once
#include <JuceHeader.h>

namespace ecm {

class MidiProtocol {
public:
    virtual ~MidiProtocol() = default;

    virtual void addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) = 0;
    virtual void addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) = 0;

    virtual void addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) = 0;
    virtual void addChannelPressure(juce::MidiBuffer& buffer, int channel, float value, int eventTime) = 0;
    virtual void addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) = 0;
    virtual void addCC(juce::MidiBuffer& buffer, int channel, int ccNumber, float value, int eventTime) = 0;

    virtual void addProgramChange(juce::MidiBuffer& buffer, int channel, int program, int eventTime) = 0;
    virtual void addAllNotesOff(juce::MidiBuffer& buffer, int channel, int eventTime) = 0;

    virtual void addMidiStart(juce::MidiBuffer& buffer, int eventTime) = 0;
    virtual void addMidiStop(juce::MidiBuffer& buffer, int eventTime) = 0;
    virtual void addMidiContinue(juce::MidiBuffer& buffer, int eventTime) = 0;

    virtual void setup(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) = 0;
};

}
