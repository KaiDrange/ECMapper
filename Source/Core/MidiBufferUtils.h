#pragma once

#include <JuceHeader.h>
#include <cstring>

namespace ecm {

// MidiBuffer::addEvent/addEvents interpret their input as MIDI 1.0 bytes.
// Preserve packet lengths when using MidiBuffer as storage for UMP packets.
inline void addRawMidiEvent(juce::MidiBuffer& buffer, const void* data, int numBytes, int sampleNumber) {
    jassert(numBytes > 0 && numBytes <= 65535);
    if (numBytes <= 0 || numBytes > 65535) return;
    constexpr int headerSize = sizeof(juce::int32) + sizeof(juce::uint16);
    int offset = 0;
    while (offset < buffer.data.size()) {
        const auto* event = buffer.data.begin() + offset;
        if (juce::readUnaligned<juce::int32>(event) > sampleNumber) break;
        offset += headerSize + juce::readUnaligned<juce::uint16>(event + sizeof(juce::int32));
    }
    buffer.data.insertMultiple(offset, 0, headerSize + numBytes);
    auto* destination = buffer.data.begin() + offset;
    juce::writeUnaligned<juce::int32>(destination, sampleNumber);
    juce::writeUnaligned<juce::uint16>(destination + sizeof(juce::int32), static_cast<juce::uint16>(numBytes));
    std::memcpy(destination + headerSize, data, static_cast<size_t>(numBytes));
}

inline void appendRawMidiBuffer(juce::MidiBuffer& destination, const juce::MidiBuffer& source, int sampleDelta = 0) {
    jassert(&destination != &source);
    for (const auto metadata : source)
        if (metadata.samplePosition >= 0)
            addRawMidiEvent(destination, metadata.data, metadata.numBytes, metadata.samplePosition + sampleDelta);
}

} // namespace ecm
