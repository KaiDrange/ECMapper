#include "Midi1Protocol.h"

namespace ecm {

Midi1Protocol::Midi1Protocol() {
    juce::Logger::writeToLog("Midi1Protocol: Created");
}

void Midi1Protocol::renderEvent(juce::MidiBuffer& buffer, const PerformanceEvent& event) {
    switch (event.kind) {
        case PerformanceEventKind::NoteOn:
            addNoteOn(buffer, event.channel, event.noteNumber, event.velocity, event.sampleOffset);
            break;
        case PerformanceEventKind::NoteOff:
            addNoteOff(buffer, event.channel, event.noteNumber, event.velocity, event.sampleOffset);
            break;
        case PerformanceEventKind::PitchBend:
            addPitchBend(buffer, event.channel, event.perNote ? event.noteNumber : -1, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::ChannelPressure:
            addChannelPressure(buffer, event.channel, event.perNote ? event.noteNumber : -1, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::PolyAftertouch:
            addPolyAftertouch(buffer, event.channel, event.noteNumber, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::Controller:
            addCC(buffer, event.channel, event.perNote ? event.noteNumber : -1, event.controller, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::ProgramChange:
            addProgramChange(buffer, event.channel, event.program, event.sampleOffset);
            break;
        case PerformanceEventKind::AllNotesOff:
            addAllNotesOff(buffer, event.channel, event.sampleOffset);
            break;
        case PerformanceEventKind::MidiStart:
            addMidiStart(buffer, event.sampleOffset);
            break;
        case PerformanceEventKind::MidiStop:
            addMidiStop(buffer, event.sampleOffset);
            break;
        case PerformanceEventKind::MidiContinue:
            addMidiContinue(buffer, event.sampleOffset);
            break;
    }
}

void Midi1Protocol::addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    buffer.addEvent(juce::MidiMessage::noteOn(channel, noteNumber, velocity), eventTime);
}

void Midi1Protocol::addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    buffer.addEvent(juce::MidiMessage::noteOff(channel, noteNumber, velocity), eventTime);
}

void Midi1Protocol::addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    // value is expected to be in range [0, 1] (where 0.5 is center)
    // MIDI 1.0 Pitch Wheel is 14-bit: 0 to 16383. Center is 8192.
    int pb = std::clamp(static_cast<int>(value * 16384.0f), 0, 16383);
    juce::Logger::writeToLog("Midi1Protocol: Pitch Wheel - channel=" + juce::String(channel) + 
                             ", value=" + juce::String(value) + ", pb=" + juce::String(pb) + 
                             " (offset=" + juce::String(pb - 8192) + ")");
    buffer.addEvent(juce::MidiMessage::pitchWheel(channel, pb), eventTime);
}

void Midi1Protocol::addChannelPressure(juce::MidiBuffer& buffer, int channel, int /*noteNumber*/, float value, int eventTime) {
    int val = std::clamp(static_cast<int>(value * 128.0f), 0, 127);
    buffer.addEvent(juce::MidiMessage::channelPressureChange(channel, val), eventTime);
}

void Midi1Protocol::addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    int val = std::clamp(static_cast<int>(value * 128.0f), 0, 127);
    buffer.addEvent(juce::MidiMessage::aftertouchChange(channel, noteNumber, val), eventTime);
}

void Midi1Protocol::addCC(juce::MidiBuffer& buffer, int channel, int /*noteNumber*/, int ccNumber, float value, int eventTime) {
    int val = std::clamp(static_cast<int>(value * 128.0f), 0, 127);
    buffer.addEvent(juce::MidiMessage::controllerEvent(channel, ccNumber, val), eventTime);
}

void Midi1Protocol::addProgramChange(juce::MidiBuffer& buffer, int channel, int program, int eventTime) {
    buffer.addEvent(juce::MidiMessage::programChange(channel, program), eventTime);
}

void Midi1Protocol::addAllNotesOff(juce::MidiBuffer& buffer, int channel, int eventTime) {
    buffer.addEvent(juce::MidiMessage::allNotesOff(channel), eventTime);
}

void Midi1Protocol::addMidiStart(juce::MidiBuffer& buffer, int eventTime) {
    buffer.addEvent(juce::MidiMessage::midiStart(), eventTime);
}

void Midi1Protocol::addMidiStop(juce::MidiBuffer& buffer, int eventTime) {
    buffer.addEvent(juce::MidiMessage::midiStop(), eventTime);
}

void Midi1Protocol::addMidiContinue(juce::MidiBuffer& buffer, int eventTime) {
    buffer.addEvent(juce::MidiMessage::midiContinue(), eventTime);
}

void Midi1Protocol::setupTransport(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) {
    juce::Logger::writeToLog("Midi1Protocol: Setting up MPE Zone Layout. Lower channels: " + juce::String(layout.getLowerZone().numMemberChannels) + 
                             ", Lower PB: " + juce::String(layout.getLowerZone().perNotePitchbendRange) +
                             ", Upper channels: " + juce::String(layout.getUpperZone().numMemberChannels) +
                             ", Upper PB: " + juce::String(layout.getUpperZone().perNotePitchbendRange));
    auto buff = juce::MPEMessages::setZoneLayout(layout);
    buffer.addEvents(buff, 0, -1, 0);
}

void Midi1Protocol::addIdentification(juce::MidiBuffer&, int) {
}

}
