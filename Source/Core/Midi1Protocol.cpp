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
    // value is expected to be in range [0, 1] (where 0.5 is center)
    // MIDI 1.0 Pitch Wheel is 14-bit: 0 to 16383
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
    if (lowerChanAssigner_) lowerChanAssigner_->allNotesOff();
    if (upperChanAssigner_) upperChanAssigner_->allNotesOff();
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
    
    lowerChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(layout.getLowerZone());
    if (layout.getUpperZone().numMemberChannels > 0)
        upperChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(layout.getUpperZone());
    else
        upperChanAssigner_.reset();
}

void Midi1Protocol::addIdentification(juce::MidiBuffer&, int) {
}

int Midi1Protocol::findMidiChannelForNewNote(MidiChannelType outputType, int noteNumber) {
    if (outputType == MidiChannelType::MPE_Low) {
        if (lowerChanAssigner_ && noteNumber != -1)
            return lowerChanAssigner_->findMidiChannelForNewNote(noteNumber);
        return 1;
    }
    if (outputType == MidiChannelType::MPE_High) {
        if (upperChanAssigner_ && noteNumber != -1)
            return upperChanAssigner_->findMidiChannelForNewNote(noteNumber);
        return 16;
    }
    
    return static_cast<int>(outputType);
}

void Midi1Protocol::releaseMidiChannel(MidiChannelType outputType, int noteNumber, int channel) {
    if (outputType == MidiChannelType::MPE_Low && lowerChanAssigner_ && noteNumber != -1)
        lowerChanAssigner_->noteOff(noteNumber, channel);
    else if (outputType == MidiChannelType::MPE_High && upperChanAssigner_ && noteNumber != -1)
        upperChanAssigner_->noteOff(noteNumber, channel);
}

}
