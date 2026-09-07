#include "Midi2Protocol.h"
#include <cstring>

namespace ecm {

using namespace juce::universal_midi_packets;

Midi2Protocol::Midi2Protocol(uint8_t group) : group_(group) {
    juce::Logger::writeToLog("Midi2Protocol: Created for group " + juce::String((int)group));
}

// Helper to bypass MidiBuffer::addEvent validation which fails for UMP in JUCE 9.0.1
static void addToBuffer(juce::MidiBuffer& buffer, const uint32_t* data, int numWords, int sampleNumber) {
    int numBytes = numWords * 4;
    int offset = 0;
    
    // Manual search for insertion point to keep MidiBuffer sorted
    const uint8_t* b = buffer.data.begin();
    const uint8_t* e = buffer.data.end();
    while (b + (int)sizeof(juce::int32) + (int)sizeof(juce::uint16) <= e) {
        int eventTime = juce::readUnaligned<juce::int32>(b);
        if (eventTime > sampleNumber) break;
        int size = juce::readUnaligned<juce::uint16>(b + sizeof(juce::int32));
        int total = (int)sizeof(juce::int32) + (int)sizeof(juce::uint16) + size;
        if (b + total > e) break;
        offset += total;
        b += total;
    }
    
    buffer.data.insertMultiple(offset, 0, numBytes + (int)sizeof(juce::int32) + (int)sizeof(juce::uint16));
    uint8_t* dest = buffer.data.begin() + offset;
    juce::writeUnaligned<juce::int32>(dest, sampleNumber);
    dest += sizeof(juce::int32);
    juce::writeUnaligned<juce::uint16>(dest, (juce::uint16)numBytes);
    dest += sizeof(juce::uint16);
    std::memcpy(dest, data, (size_t)numBytes);
}

static uint32_t scaleTo32Bit(float value) {
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(static_cast<double>(value) * 4294967296.0);
}

static uint16_t scaleTo16Bit(float value) {
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 0xFFFFu;
    return static_cast<uint16_t>(static_cast<double>(value) * 65536.0);
}

void Midi2Protocol::addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    auto ump = Factory::makeNoteOnV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, Factory::NoteAttributeKind::none, scaleTo16Bit(velocity), 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    auto ump = Factory::makeNoteOffV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, Factory::NoteAttributeKind::none, scaleTo16Bit(velocity), 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    if (noteNumber == -1) {
        auto ump = Factory::makePitchBendV2(group_, (uint8_t)(channel - 1), scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        auto ump = Factory::makePerNotePitchBendV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    }
}

void Midi2Protocol::addChannelPressure(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    if (noteNumber == -1) {
        auto ump = Factory::makeChannelPressureV2(group_, (uint8_t)(channel - 1), scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        auto ump = Factory::makePolyPressureV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    }
}

void Midi2Protocol::addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    auto ump = Factory::makePolyPressureV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, scaleTo32Bit(value));
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addCC(juce::MidiBuffer& buffer, int channel, int noteNumber, int ccNumber, float value, int eventTime) {
    if (noteNumber == -1) {
        auto ump = Factory::makeControlChangeV2(group_, (uint8_t)(channel - 1), (uint8_t)ccNumber, scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        if (ccNumber == 74 || ccNumber == 71 || ccNumber == 1 || ccNumber == 2 || ccNumber == 7 || ccNumber == 10 || ccNumber == 11) {
            auto ump = Factory::makeRegisteredPerNoteControllerV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, (uint8_t)ccNumber, scaleTo32Bit(value));
            addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
        } else {
            auto ump = Factory::makeAssignablePerNoteControllerV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, (uint8_t)ccNumber, scaleTo32Bit(value));
            addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
        }
    }
}

void Midi2Protocol::addProgramChange(juce::MidiBuffer& buffer, int channel, int program, int eventTime) {
    auto ump = Factory::makeProgramChangeV2(group_, (uint8_t)(channel - 1), 0, (uint8_t)program, 0, 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addAllNotesOff(juce::MidiBuffer& buffer, int channel, int eventTime) {
    addCC(buffer, channel, -1, 123, 0.0f, eventTime);
}

void Midi2Protocol::addMidiStart(juce::MidiBuffer& buffer, int eventTime) {
    auto ump = Factory::makeStart(group_);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addMidiStop(juce::MidiBuffer& buffer, int eventTime) {
    auto ump = Factory::makeStop(group_);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addMidiContinue(juce::MidiBuffer& buffer, int eventTime) {
    auto ump = Factory::makeContinue(group_);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::setup(juce::MidiBuffer&, const juce::MPEZoneLayout& layout) {
    lowerChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(layout.getLowerZone());
    if (layout.getUpperZone().numMemberChannels > 0)
        upperChanAssigner_ = std::make_unique<juce::MPEChannelAssigner>(layout.getUpperZone());
    else
        upperChanAssigner_.reset();
}

void Midi2Protocol::addIdentification(juce::MidiBuffer& buffer, int eventTime) {
    using namespace juce::universal_midi_packets;

    DeviceInfo devInfo;
    devInfo.manufacturer = {std::byte{0x7D}, std::byte{0x00}, std::byte{0x00}};
    devInfo.family = {std::byte{0x01}, std::byte{0x00}};
    devInfo.modelNumber = {std::byte{0x01}, std::byte{0x00}};
    devInfo.revision = {std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

    EndpointInfo info;
    info = info.withVersion(1, 1)
               .withMidi1Support(true)
               .withMidi2Support(true)
               .withStaticFunctionBlocks(true)
               .withNumFunctionBlocks(1)
               .withReceiveJRSupport(false)
               .withTransmitJRSupport(false);
    
    auto infoPkt = Factory::makeEndpointInfoNotification(info);
    addToBuffer(buffer, infoPkt.data(), (int)infoPkt.size(), eventTime);

    auto idNotification = Factory::makeDeviceIdentityNotification(devInfo);
    addToBuffer(buffer, idNotification.data(), (int)idNotification.size(), eventTime);

    Factory::makeEndpointNameNotification("ECMapper Direct UMP", [&](const View& view) {
        addToBuffer(buffer, view.data(), (int)view.size(), eventTime);
    });

    Factory::makeProductInstanceIdNotification("ECMapper-Direct-UMP", [&](const View& view) {
        addToBuffer(buffer, view.data(), (int)view.size(), eventTime);
    });
    
    BlockInfo block;
    block = block.withFirstGroup(0)
                 .withNumGroups(16)
                 .withDirection(BlockDirection::bidirectional)
                 .withUiHint(BlockUiHint::bidirectional)
                 .withEnabled(true);
    
    auto blockPkt = Factory::makeFunctionBlockInfoNotification(0, block);
    addToBuffer(buffer, blockPkt.data(), (int)blockPkt.size(), eventTime);

    Factory::makeFunctionBlockNameNotification(0, "Main", [&](const View& view) {
        addToBuffer(buffer, view.data(), (int)view.size(), eventTime);
    });

    StreamConfiguration config;
    config = config.withProtocol(PacketProtocol::MIDI_2_0)
                   .withReceiveTimestamp(false)
                   .withTransmitTimestamp(false);
    auto configPkt = Factory::makeStreamConfigurationNotification(config);
    addToBuffer(buffer, configPkt.data(), (int)configPkt.size(), eventTime);
}

int Midi2Protocol::findMidiChannelForNewNote(MidiChannelType outputType, int noteNumber) {
    if (remoteSupportsPerNote_) {
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

void Midi2Protocol::releaseMidiChannel(MidiChannelType outputType, int noteNumber, int channel) {
    if (remoteSupportsPerNote_) return;

    if (outputType == MidiChannelType::MPE_Low && lowerChanAssigner_ && noteNumber != -1)
        lowerChanAssigner_->noteOff(noteNumber, channel);
    else if (outputType == MidiChannelType::MPE_High && upperChanAssigner_ && noteNumber != -1)
        upperChanAssigner_->noteOff(noteNumber, channel);
}

void Midi2Protocol::setRemoteSupportsPerNote(bool supports) {
    remoteSupportsPerNote_ = supports;
    juce::Logger::writeToLog("Midi2Protocol: Remote supports per-note expression: " + juce::String(supports ? "Yes" : "No"));
}

}
