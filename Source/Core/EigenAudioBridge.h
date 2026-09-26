#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace ecm {

// Sample-clocked metronome and 48 kHz transport bridge. JUCE's audio callback is
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
        uint64_t generation = 0;
        uint64_t transportState = 0;
    };

    void prepare(double hostSampleRate);
    void setTiming(double bpm, int beatsPerBar, int beatUnit) noexcept;
    void setHostActive(bool active) noexcept;
    juce::String start(); // Non-audio thread. Empty string means success.
    void stop() noexcept;
    bool isPlaying() const noexcept;
    void setVolume(float volume) noexcept;
    void process(int numFrames, bool nonRealtime = false) noexcept;
    uint64_t transportState() const noexcept { return transportState_.load(); }
    bool isNonRealtime() const noexcept { return (transportState() & 1) != 0; }
    bool pop(Block& block) noexcept;
    juce::String diagnosticSummary() const; // Non-audio thread only.
    uint64_t droppedBlocks() const noexcept { return dropped_.load(); }

private:
    void requestPlayback(bool play) noexcept;
    std::array<juce::AudioBuffer<float>, 2> clicks_;
    std::atomic<double> bpm_ { 120.0 };
    std::atomic<int> meter_ { (4 << 8) | 4 };
    double beatPhase_ = 0.0;
    int beat_ = 0;
    int click_ = 0;
    juce::AbstractFifo fifo_ { queueBlocks };
    std::array<Block, queueBlocks> queue_;
    Block accumulator_;
    int accumulated_ = 0;
    int position_ = 0;
    uint64_t observedGeneration_ = 0;
    // Low bit = offline; increments on each transition to invalidate queued audio.
    std::atomic<uint64_t> transportState_ { 0 };
    std::atomic<uint64_t> command_ { 0 }; // Low bit = play; upper bits = generation.
    std::atomic<uint64_t> callbackCount_ { 0 };
    std::atomic<int> playbackPosition_ { 0 };
    std::atomic<double> hostSampleRate_ { 0.0 };
    std::atomic<uint64_t> dropped_ { 0 };
    std::atomic<bool> hostActive_ { false };
    std::atomic<bool> ready_ { false };
    std::atomic<float> volume_ { 1.0f };
    juce::SmoothedValue<float> gain_;
    juce::CriticalSection preparationLock_;
    juce::String error_ { "Audio is not running. Select a 48 kHz audio device first." };
};

} // namespace ecm
