#include "Midi2Protocol.h"
#include <cstring>

namespace ecm {

using namespace juce::universal_midi_packets;

Midi2Protocol::Midi2Protocol(uint8_t group) : group_(group) {
    juce::Logger::writeToLog("Midi2Protocol: Created for group " + juce::String((int)group));
}

void Midi2Protocol::renderEvent(juce::MidiBuffer& buffer, const PerformanceEvent& event) {
    const auto group = groupForEvent(event);

    switch (event.kind) {
        case PerformanceEventKind::NoteOn:
            addNoteOn(buffer, group, event.channel, event.noteNumber, event.velocity, event.sampleOffset);
            break;
        case PerformanceEventKind::NoteOff:
            addNoteOff(buffer, group, event.channel, event.noteNumber, event.velocity, event.sampleOffset);
            break;
        case PerformanceEventKind::PitchBend:
            addPitchBend(buffer, group, event.channel, event.perNote ? event.noteNumber : -1, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::ChannelPressure:
            addChannelPressure(buffer, group, event.channel, event.perNote ? event.noteNumber : -1, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::PolyAftertouch:
            addPolyAftertouch(buffer, group, event.channel, event.noteNumber, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::Controller:
            addCC(buffer, group, event.channel, event.perNote ? event.noteNumber : -1, event.controller, event.value, event.sampleOffset);
            break;
        case PerformanceEventKind::ProgramChange:
            addProgramChange(buffer, group, event.channel, event.program, event.sampleOffset);
            break;
        case PerformanceEventKind::AllNotesOff:
            addAllNotesOff(buffer, group, event.channel, event.sampleOffset);
            break;
        case PerformanceEventKind::MidiStart:
            addMidiStart(buffer, group, event.sampleOffset);
            break;
        case PerformanceEventKind::MidiStop:
            addMidiStop(buffer, group, event.sampleOffset);
            break;
        case PerformanceEventKind::MidiContinue:
            addMidiContinue(buffer, group, event.sampleOffset);
            break;
    }
}

uint8_t Midi2Protocol::groupForEvent(const PerformanceEvent& event) const {
    if (event.zoneIndex >= 0 && event.zoneIndex < 3)
        return static_cast<uint8_t>(event.zoneIndex);

    return group_;
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

void Midi2Protocol::addNoteOn(juce::MidiBuffer& buffer, uint8_t group, int channel, int noteNumber, float velocity, int eventTime) {
    auto ump = Factory::makeNoteOnV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, Factory::NoteAttributeKind::none, scaleTo16Bit(velocity), 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addNoteOff(juce::MidiBuffer& buffer, uint8_t group, int channel, int noteNumber, float velocity, int eventTime) {
    auto ump = Factory::makeNoteOffV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, Factory::NoteAttributeKind::none, scaleTo16Bit(velocity), 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addPitchBend(juce::MidiBuffer& buffer, uint8_t group, int channel, int noteNumber, float value, int eventTime) {
    uint32_t scaled = scaleTo32Bit(value);
    if (noteNumber == -1) {
        juce::Logger::writeToLog("Midi2Protocol: Channel Pitch Bend - channel=" + juce::String(channel) + 
                                 ", value=" + juce::String(value) + ", scaled=" + juce::String((juce::int64)scaled));
        auto ump = Factory::makePitchBendV2(group, (uint8_t)(channel - 1), scaled);
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        juce::Logger::writeToLog("Midi2Protocol: Per-Note Pitch Bend - channel=" + juce::String(channel) + 
                                 ", note=" + juce::String(noteNumber) + ", value=" + juce::String(value) + 
                                 ", scaled=" + juce::String((juce::int64)scaled));
        auto ump = Factory::makePerNotePitchBendV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, scaled);
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    }
}

void Midi2Protocol::addChannelPressure(juce::MidiBuffer& buffer, uint8_t group, int channel, int noteNumber, float value, int eventTime) {
    if (noteNumber == -1) {
        auto ump = Factory::makeChannelPressureV2(group, (uint8_t)(channel - 1), scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        auto ump = Factory::makePolyPressureV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    }
}

void Midi2Protocol::addPolyAftertouch(juce::MidiBuffer& buffer, uint8_t group, int channel, int noteNumber, float value, int eventTime) {
    auto ump = Factory::makePolyPressureV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, scaleTo32Bit(value));
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addCC(juce::MidiBuffer& buffer, uint8_t group, int channel, int noteNumber, int ccNumber, float value, int eventTime) {
    if (noteNumber == -1) {
        auto ump = Factory::makeControlChangeV2(group, (uint8_t)(channel - 1), (uint8_t)ccNumber, scaleTo32Bit(value));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        if (ccNumber == 74 || ccNumber == 71 || ccNumber == 1 || ccNumber == 2 || ccNumber == 7 || ccNumber == 10 || ccNumber == 11) {
            auto ump = Factory::makeRegisteredPerNoteControllerV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, (uint8_t)ccNumber, scaleTo32Bit(value));
            addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
        } else {
            auto ump = Factory::makeAssignablePerNoteControllerV2(group, (uint8_t)(channel - 1), (uint8_t)noteNumber, (uint8_t)ccNumber, scaleTo32Bit(value));
            addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
        }
    }
}

void Midi2Protocol::addProgramChange(juce::MidiBuffer& buffer, uint8_t group, int channel, int program, int eventTime) {
    auto ump = Factory::makeProgramChangeV2(group, (uint8_t)(channel - 1), 0, (uint8_t)program, 0, 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addAllNotesOff(juce::MidiBuffer& buffer, uint8_t group, int channel, int eventTime) {
    addCC(buffer, group, channel, -1, 123, 0.0f, eventTime);
}

void Midi2Protocol::addMidiStart(juce::MidiBuffer& buffer, uint8_t group, int eventTime) {
    auto ump = Factory::makeStart(group);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addMidiStop(juce::MidiBuffer& buffer, uint8_t group, int eventTime) {
    auto ump = Factory::makeStop(group);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addMidiContinue(juce::MidiBuffer& buffer, uint8_t group, int eventTime) {
    auto ump = Factory::makeContinue(group);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::setupTransport(juce::MidiBuffer& buffer, const juce::MPEZoneLayout& layout) {
    juce::Logger::writeToLog("Midi2Protocol: Setting up MPE Zone Layout. Lower channels: " + juce::String(layout.getLowerZone().numMemberChannels) + 
                             ", Lower PB: " + juce::String(layout.getLowerZone().perNotePitchbendRange) +
                             ", Upper channels: " + juce::String(layout.getUpperZone().numMemberChannels) +
                             ", Upper PB: " + juce::String(layout.getUpperZone().perNotePitchbendRange));
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
               .withNumFunctionBlocks(3)
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
    
    for (uint8_t zoneIndex = 0; zoneIndex < 3; ++zoneIndex)
    {
        BlockInfo block;
        block = block.withFirstGroup(zoneIndex)
                     .withNumGroups(1)
                     .withDirection(BlockDirection::bidirectional)
                     .withUiHint(BlockUiHint::bidirectional)
                     .withEnabled(true);

        auto blockPkt = Factory::makeFunctionBlockInfoNotification(zoneIndex, block);
        addToBuffer(buffer, blockPkt.data(), (int)blockPkt.size(), eventTime);

        Factory::makeFunctionBlockNameNotification(zoneIndex,
                                                   "Zone " + juce::String((int) zoneIndex + 1),
                                                   [&](const View& view) {
                                                       addToBuffer(buffer, view.data(), (int)view.size(), eventTime);
                                                   });
    }

    StreamConfiguration config;
    config = config.withProtocol(PacketProtocol::MIDI_2_0)
                   .withReceiveTimestamp(false)
                   .withTransmitTimestamp(false);
    auto configPkt = Factory::makeStreamConfigurationNotification(config);
    addToBuffer(buffer, configPkt.data(), (int)configPkt.size(), eventTime);
}

}
