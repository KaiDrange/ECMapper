#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace ecm {

// Temporary WAV source and 48 kHz transport bridge. JUCE's audio callback is
// the sole producer; HardwareService is the sole consumer. No USB, file I/O,
// allocation or locks on the producer path. prepare() requires both stopped.
class EigenAudioBridge {
public:
    static constexpr int blockFrames = 512;
    static constexpr int queueBlocks = 8;
    struct Block {
        std::array<float, blockFrames * 2> stereo {};
        uint64_t generation = 0;
    };

    void prepare(const juce::File& file, double hostSampleRate);
    void setHostActive(bool active) noexcept;
    juce::String start(); // Non-audio thread. Empty string means success.
    void stop() noexcept;
    bool isPlaying() const noexcept;
    void setVolume(float volume) noexcept;
    void process(int numFrames) noexcept;
    bool pop(Block& block) noexcept;
    juce::String diagnosticSummary() const; // Non-audio thread only.
    uint64_t droppedBlocks() const noexcept { return dropped_.load(); }

private:
    void requestPlayback(bool play) noexcept;
    juce::AudioBuffer<float> source_;
    juce::AbstractFifo fifo_ { queueBlocks };
    std::array<Block, queueBlocks> queue_;
    Block accumulator_;
    int accumulated_ = 0;
    int position_ = 0;
    uint64_t observedGeneration_ = 0;
    std::atomic<uint64_t> command_ { 0 }; // Low bit = play; upper bits = generation.
    std::atomic<uint64_t> finishedGeneration_ { 0 };
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
