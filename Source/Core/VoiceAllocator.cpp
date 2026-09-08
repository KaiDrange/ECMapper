#include "VoiceAllocator.h"

namespace ecm {

ChannelVoiceRouter::ChannelVoiceRouter(OutputTransportMode mode)
    : mode_(mode) {
}

void ChannelVoiceRouter::configureLayout(const juce::MPEZoneLayout& layout) {
    lowerChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(layout.getLowerZone());
    if (layout.getUpperZone().numMemberChannels > 0)
        upperChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(layout.getUpperZone());
    else
        upperChanAssigner_.reset();
}

int ChannelVoiceRouter::findMidiChannelForNewNote(MidiChannelType outputType, int noteNumber) {
    if (mode_ == OutputTransportMode::UmpMidi && remoteSupportsPerNote_) {
        if (outputType == MidiChannelType::MPE_Low) return 1;
        if (outputType == MidiChannelType::MPE_High) return 16;
        return static_cast<int>(outputType);
    }

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

void ChannelVoiceRouter::releaseMidiChannel(MidiChannelType outputType, int noteNumber, int channel) {
    if (mode_ == OutputTransportMode::UmpMidi && remoteSupportsPerNote_)
        return;

    if (outputType == MidiChannelType::MPE_Low && lowerChanAssigner_ && noteNumber != -1)
        lowerChanAssigner_->noteOff(noteNumber, channel);
    else if (outputType == MidiChannelType::MPE_High && upperChanAssigner_ && noteNumber != -1)
        upperChanAssigner_->noteOff(noteNumber, channel);
}

void ChannelVoiceRouter::reset() {
    if (lowerChanAssigner_)
        lowerChanAssigner_->allNotesOff();
    if (upperChanAssigner_)
        upperChanAssigner_->allNotesOff();
}

void ChannelVoiceRouter::setRemoteSupportsPerNote(bool supports) {
    remoteSupportsPerNote_ = supports;
}

}