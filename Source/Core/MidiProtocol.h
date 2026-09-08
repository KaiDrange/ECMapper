#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "PerformanceEvent.h"

namespace ecm {

class MidiProtocol {
public:
    virtual ~MidiProtocol() = default;

    virtual void renderEvent(juce::MidiBuffer& buffer, const PerformanceEvent& event) = 0;
};

class MidiTransportSession {
public:
    virtual ~MidiTransportSession() = default;

    virtual void setupTransport(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) = 0;
    virtual void addIdentification(juce::MidiBuffer& buffer, int eventTime) = 0;
};

class MidiVoiceRouter {
public:
    virtual ~MidiVoiceRouter() = default;

    virtual void configureLayout(const juce::MPEZoneLayout& layout) = 0;
    virtual int findMidiChannelForNewNote(MidiChannelType outputType, int noteNumber) = 0;
    virtual void releaseMidiChannel(MidiChannelType outputType, int noteNumber, int channel) = 0;
    virtual void reset() = 0;
    virtual void setRemoteSupportsPerNote(bool supports) = 0;
};

}
