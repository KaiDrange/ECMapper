#pragma once

#include "MidiProtocol.h"

namespace ecm {

class ChannelVoiceRouter final : public MidiVoiceRouter {
public:
    explicit ChannelVoiceRouter(OutputTransportMode mode);

    void configureLayout(const juce::MPEZoneLayout& layout) override;
    int findMidiChannelForNewNote(MidiChannelType outputType, int noteNumber) override;
    void releaseMidiChannel(MidiChannelType outputType, int noteNumber, int channel) override;
    void reset() override;
    void setRemoteSupportsPerNote(bool supports) override;

private:
    OutputTransportMode mode_ = OutputTransportMode::LegacyMidi;
    std::atomic<bool> remoteSupportsPerNote_ { false };
    std::unique_ptr<juce::MPEChannelAssigner> lowerChanAssigner_;
    std::unique_ptr<juce::MPEChannelAssigner> upperChanAssigner_;
};

}