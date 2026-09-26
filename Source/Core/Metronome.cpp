#include "Metronome.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <BinaryData.h>
#include <cmath>
#include <limits>

namespace ecm {

juce::String Metronome::prepare(const double hostSampleRate) {
    reset();
    for (auto& click : clicks_) click.setSize(0, 0);
    gain_.reset(48000.0, 0.01);
    gain_.setCurrentAndTargetValue(volume_.load());
    if (hostSampleRate != 48000.0) {
        return "The metronome requires a 48 kHz audio device or host session.";
    }
    const char* data[] = { BinaryData::click1_wav, BinaryData::click2_wav };
    const int sizes[] = { BinaryData::click1_wavSize, BinaryData::click2_wavSize };
    for (int i = 0; i < 2; ++i) {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(
            new juce::MemoryInputStream(data[i], static_cast<size_t>(sizes[i]), false), true));
        if (!reader || reader->sampleRate != 48000.0 || reader->numChannels != 1
            || reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max()) {
            return "Metronome clicks must be mono, 48 kHz WAVs.";
        }
        auto& click = clicks_[static_cast<size_t>(i)];
        click.setSize(1, static_cast<int>(reader->lengthInSamples));
        if (!reader->read(&click, 0, click.getNumSamples(), 0, true, false)) {
            return "Could not decode the metronome click.";
        }
    }
    return {};
}

void Metronome::setVolume(const float volume) noexcept {
    volume_.store(juce::jlimit(0.0f, 1.0f, volume));
}

void Metronome::setTiming(const double bpm, const int beatsPerBar, const int beatUnit) noexcept {
    bpm_.store(std::isfinite(bpm) ? juce::jlimit(20.0, 300.0, bpm) : 120.0);
    meter_.store((juce::jlimit(0, 12, beatsPerBar) << 8) | (beatUnit == 8 ? 8 : 4));
}

void Metronome::reset() noexcept {
    midiClockPosition_ = 0;
    samplesSinceClock_ = 0;
    midiPlaying_ = false;
    midiClickActive_ = false;
    position_ = 0;
    beatPhase_ = 0.0;
    beat_ = 0;
    click_ = 0;
}

void Metronome::beginBlock() noexcept {
    gain_.setTargetValue(volume_.load());
    const int meter = meter_.load();
    beatsPerBar_ = meter >> 8;
    clocksPerBeat_ = 96 / (meter & 255);
    phaseIncrement_ = bpm_.load() * (meter & 255) / (48000.0 * 240.0);
}

void Metronome::seekQuarterNote(const double position) noexcept {
    const double beats = position * 24.0 / clocksPerBeat_;
    const double nextBeat = std::ceil(beats - 1.0e-9);
    beatPhase_ = juce::jmax(0.0, nextBeat - beats);
    beat_ = beatsPerBar_ > 0 ? static_cast<int>(std::fmod(nextBeat, beatsPerBar_)) : 0;
    position_ = clicks_[static_cast<size_t>(click_)].getNumSamples();
}

float Metronome::nextSample(const bool play) noexcept {
    const float gain = gain_.getNextValue();
    float sample = 0.0f;
    if (play) {
        if (beatPhase_ <= 1.0e-10) {
            if (beatsPerBar_ > 0) beat_ %= beatsPerBar_;
            click_ = beatsPerBar_ > 0 && beat_ == 0 ? 0 : 1;
            beat_ = beatsPerBar_ > 0 ? (beat_ + 1) % beatsPerBar_ : 0;
            position_ = 0;
            beatPhase_ += 1.0;
        }
        const auto& click = clicks_[static_cast<size_t>(click_)];
        if (position_ < click.getNumSamples()) sample = click.getSample(0, position_++) * gain;
        beatPhase_ -= phaseIncrement_;
    }
    return sample;
}

void Metronome::handleMidiClock(const juce::MidiMessage& message) noexcept {
    if (message.isMidiStart()) {
        midiClockPosition_ = 0;
        midiPlaying_ = true;
        midiClickActive_ = false;
        samplesSinceClock_ = 0;
    } else if (message.isMidiContinue()) {
        midiPlaying_ = true;
        samplesSinceClock_ = 0;
    } else if (message.isMidiStop()) {
        midiPlaying_ = false;
        midiClickActive_ = false;
    } else if (message.isSongPositionPointer()) {
        // MIDI song positions are six clocks (one sixteenth note).
        midiClockPosition_ = static_cast<uint64_t>(message.getSongPositionPointerMidiBeat()) * 6;
        midiClickActive_ = false;
    } else if (message.isMidiClock()) {
        samplesSinceClock_ = 0;
        if (!midiPlaying_) return;
        if (midiClockPosition_ % static_cast<uint64_t>(clocksPerBeat_) == 0) {
            const auto beat = midiClockPosition_ / static_cast<uint64_t>(clocksPerBeat_);
            click_ = beatsPerBar_ > 0 && beat % static_cast<uint64_t>(beatsPerBar_) == 0 ? 0 : 1;
            position_ = 0;
            midiClickActive_ = true;
        }
        ++midiClockPosition_;
    }
}

float Metronome::nextMidiSample() noexcept {
    const float gain = gain_.getNextValue();
    // A disconnected clock must not leave the transport showing as running.
    if (midiPlaying_ && ++samplesSinceClock_ >= 96000) {
        midiPlaying_ = false;
        midiClickActive_ = false;
    }
    const auto& click = clicks_[static_cast<size_t>(click_)];
    if (midiPlaying_ && midiClickActive_ && position_ < click.getNumSamples())
        return click.getSample(0, position_++) * gain;
    return 0.0f;
}

} // namespace ecm
