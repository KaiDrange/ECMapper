#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace ecm {

// Mono click generator, independent of Eigenharp transport and UI state.
// Timing/volume setters may run on the control thread. All other methods
// belong to the audio producer; prepare() requires rendering to be stopped.
// MIDI clock is consumed on the audio thread at its buffer sample offsets.
class Metronome {
public:
    juce::String prepare(double hostSampleRate); // Empty string means success.
    void setTiming(double bpm, int beatsPerBar, int beatUnit) noexcept;
    void setVolume(float volume) noexcept;
    double bpm() const noexcept { return bpm_.load(); }
    float volume() const noexcept { return volume_.load(); }

    void reset() noexcept; // Next playing sample starts a new bar.
    void seekQuarterNote(double position) noexcept; // After beginBlock(), when audio is enabled mid-transport.
    void beginBlock() noexcept; // Snapshot control settings once per callback.
    // Call after successful prepare() and beginBlock(). No allocation, locks or file I/O.
    float nextSample(bool play) noexcept;
    void handleMidiClock(const juce::MidiMessage& message) noexcept;
    float nextMidiSample() noexcept;
    bool midiPlaying() const noexcept { return midiPlaying_; }
    int samplePosition() const noexcept { return position_; } // Audio thread only.

private:
    std::array<juce::AudioBuffer<float>, 2> clicks_;
    std::atomic<double> bpm_ { 120.0 };
    std::atomic<int> meter_ { (4 << 8) | 4 };
    std::atomic<float> volume_ { 1.0f };
    juce::SmoothedValue<float> gain_;
    double beatPhase_ = 0.0;
    double phaseIncrement_ = 0.0;
    int beatsPerBar_ = 4;
    int beat_ = 0;
    int click_ = 0;
    int position_ = 0;
    int clocksPerBeat_ = 24;
    uint64_t midiClockPosition_ = 0;
    int samplesSinceClock_ = 0;
    bool midiPlaying_ = false;
    bool midiClickActive_ = false;
};

} // namespace ecm
