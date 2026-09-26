#include "EigenAudioBridge.h"
#include <limits>

namespace ecm {

void EigenAudioBridge::prepare(const juce::File& file, double hostSampleRate) {
    const juce::ScopedLock lock(preparationLock_);
    ready_.store(false);
    stop();
    fifo_.reset();
    accumulated_ = position_ = 0;
    observedGeneration_ = command_.load();
    dropped_.store(0);
    callbackCount_.store(0);
    playbackPosition_.store(0);
    hostSampleRate_.store(hostSampleRate);
    source_.setSize(0, 0);
    gain_.reset(48000.0, 0.01);
    gain_.setCurrentAndTargetValue(volume_.load());
    if (hostSampleRate != 48000.0) {
        error_ = "Temporary Eigenharp playback requires a 48 kHz audio device or host session.";
        return;
    }
    juce::WavAudioFormat wav;
    auto stream = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader;
    if (stream) reader.reset(wav.createReaderFor(stream.release(), true));
    if (!reader) {
        error_ = "Cannot read the test WAV: " + file.getFullPathName();
        return;
    }
    if (reader->sampleRate != 48000.0 || reader->numChannels != 2
        || reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max()) {
        error_ = "The test file must be a stereo, 48 kHz WAV.";
        return;
    }
    source_.setSize(2, static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&source_, 0, source_.getNumSamples(), 0, true, true)) {
        source_.setSize(0, 0);
        error_ = "Could not decode the test WAV.";
        return;
    }
    error_.clear();
    ready_.store(true);
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
    if (!ready_.load()) return error_;
    if (!hostActive_.load()) return "Test playback is available in Host mode only.";
    requestPlayback(true);
    return {};
}

void EigenAudioBridge::stop() noexcept { requestPlayback(false); }

bool EigenAudioBridge::isPlaying() const noexcept {
    const auto command = command_.load();
    return hostActive_.load() && (command & 1) != 0 && finishedGeneration_.load() != command;
}

void EigenAudioBridge::setVolume(float volume) noexcept {
    volume_.store(juce::jlimit(0.0f, 1.0f, volume));
}

void EigenAudioBridge::process(int numFrames) noexcept {
    callbackCount_.fetch_add(1, std::memory_order_relaxed);
    if (!ready_.load() || !hostActive_.load()) return;
    const auto command = command_.load();
    if (command != observedGeneration_) {
        observedGeneration_ = command;
        accumulated_ = position_ = 0;
    }
    gain_.setTargetValue(volume_.load());
    for (int frame = 0; frame < numFrames; ++frame) {
        const float gain = gain_.getNextValue();
        const bool play = (command & 1) != 0 && position_ < source_.getNumSamples();
        for (int channel = 0; channel < 2; ++channel)
            accumulator_.stereo[static_cast<std::size_t>(accumulated_ * 2 + channel)] =
                play ? source_.getSample(channel, position_) * gain : 0.0f;
        if (play && ++position_ == source_.getNumSamples()) finishedGeneration_.store(command);
        if (++accumulated_ == blockFrames) {
            accumulator_.generation = command;
            const auto write = fifo_.write(1);
            if (write.blockSize1 != 0) queue_[static_cast<std::size_t>(write.startIndex1)] = accumulator_;
            else dropped_.fetch_add(1);
            accumulated_ = 0;
        }
    }
    playbackPosition_.store(position_, std::memory_order_relaxed);
}

juce::String EigenAudioBridge::diagnosticSummary() const {
    return "ready=" + juce::String(ready_.load() ? 1 : 0)
        + " host=" + juce::String(hostActive_.load() ? 1 : 0)
        + " rate=" + juce::String(hostSampleRate_.load(), 0)
        + " blockFrames=" + juce::String(blockFrames)
        + " periods=1,0,0,0"
        + " playing=" + juce::String(isPlaying() ? 1 : 0)
        + " callbacks=" + juce::String(static_cast<juce::int64>(callbackCount_.load()))
        + " position=" + juce::String(playbackPosition_.load())
        + " volume=" + juce::String(volume_.load(), 3)
        + " fifoDrops=" + juce::String(static_cast<juce::int64>(dropped_.load()));
}

bool EigenAudioBridge::pop(Block& block) noexcept {
    for (int count = 0; count < queueBlocks; ++count) {
        const auto read = fifo_.read(1);
        if (read.blockSize1 == 0) return false;
        const auto& queued = queue_[static_cast<std::size_t>(read.startIndex1)];
        if (queued.generation == command_.load() && hostActive_.load()) {
            block = queued;
            return true;
        }
    }
    return false;
}

} // namespace ecm
