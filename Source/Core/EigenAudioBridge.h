#pragma once

#include "Metronome.h"
#include "MidiClockOutput.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace ecm {

// 48 kHz headphone transport bridge owning a mono metronome source. JUCE's audio callback is
// the sole producer; HardwareService is the sole consumer. No USB, file I/O,
// allocation or locks on the producer path. prepare() requires both stopped.
class EigenAudioBridge {
public:
    static constexpr int blockFrames = 128;
    static constexpr unsigned blocksPerPeriod = 512 / blockFrames;
    static_assert(512 % blockFrames == 0);
    // AbstractFifo reserves one slot: capacity is 3584 frames, with no prefill.
    static constexpr int queueBlocks = 1 + 3584 / blockFrames;
    struct Block {
        std::array<float, blockFrames * 2> stereo {};
        uint64_t audioOutputState = 0;
        uint64_t generation = 0;
        uint64_t transportState = 0;
    };

    void prepare(double hostSampleRate);
    void setTiming(const double bpm, int beatsPerBar, const int beatUnit) noexcept { metronome_.setTiming(bpm, beatsPerBar, beatUnit); }
    void setHostActive(bool active) noexcept;
    void setStandaloneClockEnabled(bool enabled) noexcept { standaloneClockEnabled_.store(enabled); }
    void setAudioOutputEnabled(bool enabled) noexcept;
    juce::String start(); // Non-audio thread. Empty string means success.
    void stop() noexcept;
    bool isPlaying() const noexcept;
    void setVolume(float volume) noexcept { metronome_.setVolume(volume); }
    void setMidiSlave(bool enabled) noexcept;
    void setMidiClockOutputEnabled(bool enabled) noexcept {
        if (midiClockOutputEnabled_.exchange(enabled) != enabled) stop();
    }
    void process(int numFrames, bool nonRealtime = false, const juce::MidiBuffer* midi = nullptr, juce::MidiBuffer* clockOutput = nullptr) noexcept;
    uint64_t transportState() const noexcept { return transportState_.load(); }
    bool isNonRealtime() const noexcept { return (transportState() & 1) != 0; }
    bool pop(Block& block) noexcept;
    juce::String diagnosticSummary() const; // Non-audio thread only.
    uint64_t droppedBlocks() const noexcept { return dropped_.load(); }

private:
    void requestPlayback(bool play) noexcept;
    Metronome metronome_;
    MidiClockMaster clockMaster_;
    std::atomic<bool> midiClockOutputEnabled_ { true };
    std::atomic<bool> midiSlave_ { false };
    std::atomic<bool> midiPlaying_ { false };
    juce::AbstractFifo fifo_ { queueBlocks };
    std::array<Block, queueBlocks> queue_;
    Block accumulator_;
    int accumulated_ = 0;
    uint64_t observedGeneration_ = 0;
    // Low bit = offline; increments on each transition to invalidate queued audio.
    std::atomic<uint64_t> transportState_ { 0 };
    std::atomic<uint64_t> command_ { 0 }; // Low bit = play; upper bits = generation.
    std::atomic<uint64_t> callbackCount_ { 0 };
    std::atomic<int> playbackPosition_ { 0 };
    std::atomic<double> hostSampleRate_ { 0.0 };
    std::atomic<uint64_t> dropped_ { 0 };
    std::atomic<bool> standaloneClockEnabled_ { false };
    // Low bit = an enabled headphone output exists; upper bits invalidate stale audio.
    std::atomic<uint64_t> audioOutputState_ { 0 };
    uint64_t observedAudioOutputState_ = 0;
    std::atomic<bool> hostActive_ { false };
    std::atomic<bool> ready_ { false };
    juce::CriticalSection preparationLock_;
    juce::String error_ { "Audio is not running. Select a 48 kHz audio device first." };
};

} // namespace ecm
