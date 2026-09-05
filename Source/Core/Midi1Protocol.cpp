#include "Midi1Protocol.h"

namespace ecm {

Midi1Protocol::Midi1Protocol() {
    juce::Logger::writeToLog("Midi1Protocol: Created");
}

void Midi1Protocol::addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    buffer.addEvent(juce::MidiMessage::noteOn(channel, noteNumber, velocity), eventTime);
}

void Midi1Protocol::addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    buffer.addEvent(juce::MidiMessage::noteOff(channel, noteNumber, velocity), eventTime);
}

void Midi1Protocol::addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    // value is expected to be in range [0, 1] for unipolar or combined, but MidiMessage::pitchWheel takes 0-16383
    // MidiService currently calculates totalPB as 0-16383.
    // Wait, I should decide if the protocol takes float or int.
    // If I use float [0, 1], I need to map it.
    // Actually, MidiService was doing:
    // int totalPB = std::clamp(currentKeyPBperChannel_[channel - 1] + currentStripPBperChannel_[channel - 1] + 8192, 0, 16383);
    // So if I pass this value as a float [0, 1], it would be (totalPB / 16383.0f).
    
    int pb = std::clamp(static_cast<int>(value * 16383.0f), 0, 16383);
    buffer.addEvent(juce::MidiMessage::pitchWheel(channel, pb), eventTime);
}

void Midi1Protocol::addChannelPressure(juce::MidiBuffer& buffer, int channel, float value, int eventTime) {
    int val = std::clamp(static_cast<int>(value * 127.0f), 0, 127);
    buffer.addEvent(juce::MidiMessage::channelPressureChange(channel, val), eventTime);
}

void Midi1Protocol::addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    int val = std::clamp(static_cast<int>(value * 127.0f), 0, 127);
    buffer.addEvent(juce::MidiMessage::aftertouchChange(channel, noteNumber, val), eventTime);
}

void Midi1Protocol::addCC(juce::MidiBuffer& buffer, int channel, int ccNumber, float value, int eventTime) {
    int val = std::clamp(static_cast<int>(value * 127.0f), 0, 127);
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

void Midi1Protocol::setup(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) {
    auto buff = juce::MPEMessages::setZoneLayout(layout);
    buffer.addEvents(buff, 0, -1, 0);
}

}
