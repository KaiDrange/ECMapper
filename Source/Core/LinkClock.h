#pragma once

#include <cstdint>
#include <memory>

namespace ecm {

// One Link participant per processor. Control methods never touch ValueTrees or
// audio buffers; session edits are committed exclusively by the audio producer.
class LinkClock {
public:
    struct Block {
        bool playing = false;
        double beat = 0.0; // Quarter notes; negative values are the quantized count-in.
        double beatsPerSample = 0.0;
    };

    LinkClock();
    ~LinkClock();
    void setEnabled(bool enabled); // Non-realtime control thread.
    bool isEnabled() const noexcept;
    void setStartStopSync(bool enabled) noexcept;
    void requestTempo(double bpm) noexcept;
    void requestStart() noexcept;
    void requestStop() noexcept;
    void resetPlayback() noexcept; // Local lifecycle stop; never stops other peers.
    double tempo() const; // Non-realtime, also works while audio is stopped.
    int peers() const noexcept;
    bool isPlaying() const noexcept;

    void prepare() noexcept; // Audio producer stopped.
    Block process(int frames, double sampleRate, double quantum, double outputLatencySeconds) noexcept;
    // Audio thread, timestamp in Link's monotonic clock domain. Also permits
    // deterministic rendering tests without sleeping for an audio callback.
    Block processAtTime(double sampleRate, double quantum, int64_t outputTimeMicros) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ecm
