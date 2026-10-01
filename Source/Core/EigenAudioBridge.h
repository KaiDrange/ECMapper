#pragma once

#include "Metronome.h"
#include "MidiClockOutput.h"
#include "LinkClock.h"
#include "HostClock.h"
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
        uint64_t routingState = 0;
        uint64_t audioOutputState = 0;
        uint64_t generation = 0;
        uint64_t transportState = 0;
    };

    void prepare(double hostSampleRate);
    void setTiming(double bpm, int beatsPerBar, int beatUnit) noexcept;
    void setLinkEnabled(bool enabled);
    bool isLinkEnabled() const noexcept { return link_.isEnabled(); }
    void setHostSyncEnabled(bool enabled) noexcept {
        if (hostSyncEnabled_.exchange(enabled) != enabled) {
            hostTimingAvailable_.store(false);
            stop(false);
        }
    }
    double hostTempo() const noexcept { return hostTempo_.load(); }
    int hostNumerator() const noexcept { return hostNumerator_.load(); }
    int hostDenominator() const noexcept { return hostDenominator_.load(); }
    bool hostTimingAvailable() const noexcept { return hostTimingAvailable_.load(); }
    void setStartStopSync(bool enabled) noexcept {
        startStopSync_.store(enabled);
        link_.setStartStopSync(enabled);
    }
    double linkTempo() const { return link_.tempo(); }
    void requestLinkTempo(double bpm) noexcept { link_.requestTempo(bpm); }
    int linkPeers() const noexcept { return link_.peers(); }
    void setOutputLatency(double seconds) noexcept { outputLatency_.store(seconds); }
    void setHostActive(bool active) noexcept;
    void setStandaloneClockEnabled(bool enabled) noexcept { standaloneClockEnabled_.store(enabled); }
    void setAudioOutputEnabled(bool enabled) noexcept;
    juce::String start(); // Non-audio thread. Empty string means success.
    void stop(bool shareWithLink = true) noexcept;
    bool isPlaying() const noexcept;
    void setVolume(float volume) noexcept { metronome_.setVolume(volume); }
    // Destination IDs: 1 = headphones, 2 = audio device/host, 3 = both, 4 = none.
    void setRouting(int metronomeRoute, int inputRoute) noexcept;
    bool metronomeUsesDeviceOutput() const noexcept { return (routingState_.load() & 2) != 0; }
    void setInputVolume(float volume) noexcept { inputVolume_.store(juce::jlimit(0.0f, 1.0f, volume)); }
    void setMidiSlave(bool enabled) noexcept;
    void setMidiClockOutputEnabled(bool enabled) noexcept {
        if (midiClockOutputEnabled_.exchange(enabled) != enabled) stop(false);
    }
    void process(int numFrames, bool nonRealtime = false, const juce::MidiBuffer* midi = nullptr, juce::MidiBuffer* clockOutput = nullptr, juce::AudioBuffer<float>* audio = nullptr, int inputChannels = 0, int outputChannels = 0, const juce::AudioPlayHead::PositionInfo* hostPosition = nullptr) noexcept;
    uint64_t transportState() const noexcept { return transportState_.load(); }
    bool isNonRealtime() const noexcept { return (transportState() & 1) != 0; }
    bool pop(Block& block) noexcept;
    juce::String diagnosticSummary() const; // Non-audio thread only.
    uint64_t droppedBlocks() const noexcept { return dropped_.load(); }

private:
    void requestPlayback(bool play) noexcept;
    void resetHeadphoneResampler() noexcept;
    // Next 48 kHz sample's position relative to the current host sample.
    double headphoneSamplePhase_ = 0.0;
    std::array<float, 2> previousHeadphoneSample_ {};
    bool hasPreviousHeadphoneSample_ = false;
    Metronome metronome_;
    LinkClock link_;
    HostClock hostClock_;
    std::atomic<bool> hostSyncEnabled_ { false };
    std::atomic<bool> hostPlaying_ { false };
    std::atomic<bool> hostTimingAvailable_ { false };
    std::atomic<double> hostTempo_ { 120.0 };
    std::atomic<int> hostNumerator_ { 4 }, hostDenominator_ { 4 };
    std::atomic<double> quantum_ { 4.0 };
    std::atomic<double> configuredBpm_ { 120.0 };
    std::atomic<double> outputLatency_ { 0.0 };
    bool wasLinkPlaying_ = false;
    std::atomic<uint64_t> routingState_ { 5 }; // Two route masks in low four bits, generation above.
    uint64_t observedRoutingState_ = 5;
    std::atomic<float> inputVolume_ { 1.0f };
    juce::SmoothedValue<float> inputGain_;
    bool wasRenderingMetronome_ = false;
    MidiClockMaster clockMaster_;
    std::atomic<bool> midiClockOutputEnabled_ { true };
    std::atomic<bool> midiSlave_ { false };
    std::atomic<bool> startStopSync_ { true };
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
    juce::String error_ { "Audio is not running. Select an audio device first." };
};

} // namespace ecm
