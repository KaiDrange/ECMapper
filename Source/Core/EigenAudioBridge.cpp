#include "EigenAudioBridge.h"

namespace ecm {

void EigenAudioBridge::prepare(double hostSampleRate) {
    const juce::ScopedLock lock(preparationLock_);
    ready_.store(false);
    stop();
    fifo_.reset();
    accumulated_ = 0;
    observedGeneration_ = command_.load();
    dropped_.store(0);
    callbackCount_.store(0);
    playbackPosition_.store(0);
    hostSampleRate_.store(hostSampleRate);
    error_ = metronome_.prepare(hostSampleRate);
    ready_.store(error_.isEmpty());
}

void EigenAudioBridge::requestPlayback(bool play) noexcept {
    auto current = command_.load();
    while (!command_.compare_exchange_weak(current, ((current + 2) & ~uint64_t(1)) | (play ? 1 : 0))) {}
}

void EigenAudioBridge::setHostActive(bool active) noexcept {
    hostActive_.store(active);
    if (!active) stop();
}

juce::String EigenAudioBridge::start() {
    const juce::ScopedLock lock(preparationLock_);
    if (isNonRealtime()) return "Eigenharp playback is disabled during offline rendering.";
    if (!ready_.load()) return error_;
    if (!hostActive_.load()) return "The metronome is available in Host mode only.";
    requestPlayback(true);
    return {};
}

void EigenAudioBridge::stop() noexcept { requestPlayback(false); }

bool EigenAudioBridge::isPlaying() const noexcept {
    const auto command = command_.load();
    return !isNonRealtime() && hostActive_.load() && (command & 1) != 0;
}

void EigenAudioBridge::process(int numFrames, bool nonRealtime) noexcept {
    // Producer only. Never reset the shared FIFO while its consumer is active.
    if (nonRealtime != isNonRealtime()) {
        transportState_.fetch_add(1);
        accumulated_ = 0;
        if (nonRealtime) stop();
    }
    if (nonRealtime) return;
    callbackCount_.fetch_add(1, std::memory_order_relaxed);
    if (!ready_.load() || !hostActive_.load()) return;
    const auto command = command_.load();
    if (command != observedGeneration_) {
        observedGeneration_ = command;
        accumulated_ = 0;
        metronome_.reset();
    }
    metronome_.beginBlock();
    for (int frame = 0; frame < numFrames; ++frame) {
        const float sample = metronome_.nextSample((command & 1) != 0);
        accumulator_.stereo[static_cast<size_t>(accumulated_ * 2)] = sample;
        accumulator_.stereo[static_cast<size_t>(accumulated_ * 2 + 1)] = sample;
        if (++accumulated_ == blockFrames) {
            accumulator_.generation = command;
            accumulator_.transportState = transportState();
            const auto write = fifo_.write(1);
            if (write.blockSize1 != 0) queue_[static_cast<std::size_t>(write.startIndex1)] = accumulator_;
            else dropped_.fetch_add(1);
            accumulated_ = 0;
        }
    }
    playbackPosition_.store(metronome_.samplePosition(), std::memory_order_relaxed);
}

juce::String EigenAudioBridge::diagnosticSummary() const {
    return "ready=" + juce::String(ready_.load() ? 1 : 0)
        + " host=" + juce::String(hostActive_.load() ? 1 : 0)
        + " rate=" + juce::String(hostSampleRate_.load(), 0)
        + " blockFrames=" + juce::String(blockFrames)
        + " periods=1,0,0,0"
        + " offline=" + juce::String(isNonRealtime() ? 1 : 0)
        + " playing=" + juce::String(isPlaying() ? 1 : 0)
        + " callbacks=" + juce::String(static_cast<juce::int64>(callbackCount_.load()))
        + " position=" + juce::String(playbackPosition_.load())
        + " volume=" + juce::String(metronome_.volume(), 3)
        + " fifoDrops=" + juce::String(static_cast<juce::int64>(dropped_.load()));
}

bool EigenAudioBridge::pop(Block& block) noexcept {
    for (int count = 0; count < queueBlocks; ++count) {
        const auto read = fifo_.read(1);
        if (read.blockSize1 == 0) return false;
        const auto& queued = queue_[static_cast<std::size_t>(read.startIndex1)];
        if (queued.generation == command_.load() && hostActive_.load()
            && !isNonRealtime() && queued.transportState == transportState()) {
            block = queued;
            return true;
        }
    }
    return false;
}

} // namespace ecm
