#include "EigenAudioBridge.h"
#include <cmath>

namespace ecm {

void EigenAudioBridge::prepare(const double hostSampleRate) {
    const juce::ScopedLock lock(preparationLock_);
    ready_.store(false);
    stop();
    clockMaster_ = {};
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

void EigenAudioBridge::requestPlayback(const bool play) noexcept {
    auto current = command_.load();
    while (!command_.compare_exchange_weak(current, ((current + 2) & ~static_cast<uint64_t>(1)) | (play ? 1 : 0))) {}
}

void EigenAudioBridge::setHostActive(const bool active) noexcept {
    hostActive_.store(active);
    if (!active && !standaloneClockEnabled_.load()) stop();
}

void EigenAudioBridge::setAudioOutputEnabled(const bool enabled) noexcept {
    auto state = audioOutputState_.load();
    while ((state & 1) != static_cast<uint64_t>(enabled)
           && !audioOutputState_.compare_exchange_weak(state, ((state + 2) & ~static_cast<uint64_t>(1)) | (enabled ? 1 : 0))) {}
}

juce::String EigenAudioBridge::start() {
    const juce::ScopedLock lock(preparationLock_);
    if (midiSlave_.load()) return "Transport follows the selected MIDI input in slave mode.";
    if (isNonRealtime()) return "Eigenharp playback is disabled during offline rendering.";
    if (standaloneClockEnabled_.load()) {
        const double rate = hostSampleRate_.load();
        if (!std::isfinite(rate) || rate <= 0.0) return "Select an audio device to run the MIDI clock.";
    } else {
        if (!ready_.load()) return error_;
        if (!hostActive_.load()) return "The metronome is available in Host mode only.";
    }
    requestPlayback(true);
    return {};
}

void EigenAudioBridge::stop() noexcept {
    midiPlaying_.store(false);
    requestPlayback(false);
}

void EigenAudioBridge::setMidiSlave(const bool enabled) noexcept {
    if (midiSlave_.exchange(enabled) != enabled) stop();
}

bool EigenAudioBridge::isPlaying() const noexcept {
    const auto command = command_.load();
    return !isNonRealtime() && (standaloneClockEnabled_.load() || hostActive_.load()) && (midiSlave_.load() ? midiPlaying_.load() : (command & 1) != 0);
}

void EigenAudioBridge::process(const int numFrames, const bool nonRealtime, const juce::MidiBuffer* midi, juce::MidiBuffer* clockOutput) noexcept {
    // Producer only. Never reset the shared FIFO while its consumer is active.
    if (nonRealtime != isNonRealtime()) {
        transportState_.fetch_add(1);
        accumulated_ = 0;
        if (nonRealtime) stop();
    }
    const auto command = command_.load();
    const double quarterNotePosition = command != observedGeneration_ ? 0.0 : clockMaster_.quarterNotePosition();
    const double rate = hostSampleRate_.load();
    if (clockOutput != nullptr)
        clockMaster_.process(numFrames, rate, metronome_.bpm(), command,
                             !nonRealtime && std::isfinite(rate) && rate > 0.0
                                 && (standaloneClockEnabled_.load() || (ready_.load() && hostActive_.load()))
                                 && !midiSlave_.load(), *clockOutput, midiClockOutputEnabled_.load());
    if (nonRealtime) return;
    callbackCount_.fetch_add(1, std::memory_order_relaxed);
    const auto audioOutputState = audioOutputState_.load();
    const bool audioChanged = audioOutputState != observedAudioOutputState_;
    observedAudioOutputState_ = audioOutputState;
    if (!ready_.load() || !hostActive_.load() || (audioOutputState & 1) == 0) {
        accumulated_ = 0;
        observedGeneration_ = command;
        midiPlaying_.store(false);
        return;
    }
    if (command != observedGeneration_) {
        observedGeneration_ = command;
        accumulated_ = 0;
        metronome_.reset();
    }
    metronome_.beginBlock();
    const bool slave = midiSlave_.load();
    if (audioChanged) {
        accumulated_ = 0;
        if (!slave && clockOutput != nullptr) metronome_.seekQuarterNote(quarterNotePosition);
        else metronome_.reset();
    }
    auto event = midi != nullptr ? midi->begin() : juce::MidiBufferIterator{};
    const auto end = midi != nullptr ? midi->end() : juce::MidiBufferIterator{};
    for (int frame = 0; frame < numFrames; ++frame) {
        if (slave) {
            while (event != end && (*event).samplePosition <= frame) {
                const auto message = *event;
                // Only short system messages are relevant; avoid copying/allocating SysEx.
                if (message.numBytes > 0 && message.numBytes <= 3 && message.data[0] >= 0xf2)
                    metronome_.handleMidiClock(message.getMessage());
                ++event;
            }
        }
        const float sample = slave ? metronome_.nextMidiSample()
                                   : metronome_.nextSample((command & 1) != 0);
        accumulator_.stereo[static_cast<size_t>(accumulated_ * 2)] = sample;
        accumulator_.stereo[static_cast<size_t>(accumulated_ * 2 + 1)] = sample;
        if (++accumulated_ == blockFrames) {
            accumulator_.audioOutputState = audioOutputState;
            accumulator_.generation = command;
            accumulator_.transportState = transportState();
            const auto write = fifo_.write(1);
            if (write.blockSize1 != 0) queue_[static_cast<std::size_t>(write.startIndex1)] = accumulator_;
            else dropped_.fetch_add(1);
            accumulated_ = 0;
        }
    }
    midiPlaying_.store(slave && metronome_.midiPlaying());
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
            && (audioOutputState_.load() & 1) != 0 && queued.audioOutputState == audioOutputState_.load()
            && !isNonRealtime() && queued.transportState == transportState()) {
            block = queued;
            return true;
        }
    }
    return false;
}

} // namespace ecm
