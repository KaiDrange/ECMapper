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
    while (b < e) {
        int eventTime = juce::readUnaligned<juce::int32>(b);
        if (eventTime > sampleNumber) break;
        int size = juce::readUnaligned<juce::uint16>(b + sizeof(juce::int32));
        int total = (int)sizeof(juce::int32) + (int)sizeof(juce::uint16) + size;
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

void Midi2Protocol::addNoteOn(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    auto ump = Factory::makeNoteOnV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, Factory::NoteAttributeKind::none, static_cast<uint16_t>(velocity * 65535.0f), 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addNoteOff(juce::MidiBuffer& buffer, int channel, int noteNumber, float velocity, int eventTime) {
    auto ump = Factory::makeNoteOffV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, Factory::NoteAttributeKind::none, static_cast<uint16_t>(velocity * 65535.0f), 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addPitchBend(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    if (noteNumber == -1) {
        auto ump = Factory::makePitchBendV2(group_, (uint8_t)(channel - 1), static_cast<uint32_t>(value * 4294967295.0f));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    } else {
        auto ump = Factory::makePerNotePitchBendV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, static_cast<uint32_t>(value * 4294967295.0f));
        addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
    }
}

void Midi2Protocol::addChannelPressure(juce::MidiBuffer& buffer, int channel, float value, int eventTime) {
    auto ump = Factory::makeChannelPressureV2(group_, (uint8_t)(channel - 1), static_cast<uint32_t>(value * 4294967295.0f));
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addPolyAftertouch(juce::MidiBuffer& buffer, int channel, int noteNumber, float value, int eventTime) {
    auto ump = Factory::makePolyPressureV2(group_, (uint8_t)(channel - 1), (uint8_t)noteNumber, static_cast<uint32_t>(value * 4294967295.0f));
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addCC(juce::MidiBuffer& buffer, int channel, int ccNumber, float value, int eventTime) {
    auto ump = Factory::makeControlChangeV2(group_, (uint8_t)(channel - 1), (uint8_t)ccNumber, static_cast<uint32_t>(value * 4294967295.0f));
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addProgramChange(juce::MidiBuffer& buffer, int channel, int program, int eventTime) {
    auto ump = Factory::makeProgramChangeV2(group_, (uint8_t)(channel - 1), 0, (uint8_t)program, 0, 0);
    addToBuffer(buffer, ump.data(), (int)ump.size(), eventTime);
}

void Midi2Protocol::addAllNotesOff(juce::MidiBuffer& buffer, int channel, int eventTime) {
    addCC(buffer, channel, 123, 0.0f, eventTime);
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

void Midi2Protocol::setup(juce::MidiBuffer&, const juce::MPEZoneLayout&) {
}

}
