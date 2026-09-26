#include "EigenAudioBridge.h"
#include <limits>
#include <cmath>
#include <BinaryData.h>

namespace ecm {

void EigenAudioBridge::prepare(double hostSampleRate) {
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
    for (auto& click : clicks_) click.setSize(0, 0);
    gain_.reset(48000.0, 0.01);
    gain_.setCurrentAndTargetValue(volume_.load());
    if (hostSampleRate != 48000.0) {
        error_ = "The metronome requires a 48 kHz audio device or host session.";
        return;
    }
    const char* data[] = { BinaryData::click1_wav, BinaryData::click2_wav };
    const int sizes[] = { BinaryData::click1_wavSize, BinaryData::click2_wavSize };
    for (int i = 0; i < 2; ++i) {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(
            new juce::MemoryInputStream(data[i], static_cast<size_t>(sizes[i]), false), true));
        if (!reader || reader->sampleRate != 48000.0 || reader->numChannels != 1
            || reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max()) {
            error_ = "Metronome clicks must be mono, 48 kHz WAVs.";
            return;
        }
        auto& click = clicks_[static_cast<size_t>(i)];
        click.setSize(1, static_cast<int>(reader->lengthInSamples));
        if (!reader->read(&click, 0, click.getNumSamples(), 0, true, false)) {
            error_ = "Could not decode the metronome click.";
            return;
        }
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

void EigenAudioBridge::setVolume(float volume) noexcept {
    volume_.store(juce::jlimit(0.0f, 1.0f, volume));
}

void EigenAudioBridge::setTiming(double bpm, int beatsPerBar, int beatUnit) noexcept {
    bpm_.store(std::isfinite(bpm) ? juce::jlimit(20.0, 300.0, bpm) : 120.0);
    meter_.store((juce::jlimit(0, 12, beatsPerBar) << 8) | (beatUnit == 8 ? 8 : 4));
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
        accumulated_ = position_ = 0;
        beatPhase_ = 0.0;
        beat_ = 0;
    }
    gain_.setTargetValue(volume_.load());
    const int meter = meter_.load();
    const int beatsPerBar = meter >> 8;
    const double phaseIncrement = bpm_.load() * (meter & 255) / (48000.0 * 240.0);
    for (int frame = 0; frame < numFrames; ++frame) {
        const float gain = gain_.getNextValue();
        const bool play = (command & 1) != 0;
        float sample = 0.0f;
        if (play) {
            if (beatPhase_ <= 1.0e-10) {
                if (beatsPerBar > 0) beat_ %= beatsPerBar;
                click_ = beatsPerBar > 0 && beat_ == 0 ? 0 : 1;
                beat_ = beatsPerBar > 0 ? (beat_ + 1) % beatsPerBar : 0;
                position_ = 0;
                beatPhase_ += 1.0;
            }
            const auto& click = clicks_[static_cast<size_t>(click_)];
            if (position_ < click.getNumSamples()) sample = click.getSample(0, position_++) * gain;
            beatPhase_ -= phaseIncrement;
        }
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
    playbackPosition_.store(position_, std::memory_order_relaxed);
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
        + " volume=" + juce::String(volume_.load(), 3)
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
