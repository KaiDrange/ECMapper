#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace ecm {

// Audio-thread-only snapshot of the DAW timeline. No calls into the host from UI.
class HostClock {
public:
    struct Block {
        bool valid = false;
        bool playing = false;
        bool discontinuity = false;
        double bpm = 120.0;
        double beat = 0.0;
        double beatsPerSample = 0.0;
        double barStart = 0.0;
        int numerator = 4;
        int denominator = 4;
    };

    Block process(const juce::AudioPlayHead::PositionInfo* position, int frames,
                  double rate, bool syncTransport, bool localPlaying) noexcept {
        Block block;
        const auto bpm = position != nullptr ? position->getBpm() : juce::Optional<double>{};
        const auto ppq = position != nullptr ? position->getPpqPosition() : juce::Optional<double>{};
        if (!bpm || !ppq || !std::isfinite(*bpm) || *bpm <= 0.0 || !std::isfinite(*ppq)
            || !std::isfinite(rate) || rate <= 0.0) {
            block.discontinuity = previous_.valid;
            previous_ = block;
            return block;
        }
        block.valid = true;
        block.bpm = *bpm;
        block.beatsPerSample = *bpm / (60.0 * rate);
        const bool hostPlaying = position->getIsPlaying();
        block.playing = syncTransport ? hostPlaying : localPlaying;
        if (const auto meter = position->getTimeSignature(); meter && meter->numerator > 0 && meter->denominator > 0) {
            block.numerator = meter->numerator;
            block.denominator = meter->denominator;
        }
        const auto bar = position->getPpqPositionOfLastBarStart();
        block.barStart = bar && std::isfinite(*bar) ? *bar : 0.0;
        const bool jumped = hostPlaying != hostWasPlaying_
            || std::abs(*ppq - expectedHostBeat_) > juce::jmax(1.0e-7, 2.0 * block.beatsPerSample);
        block.discontinuity = !previous_.valid || jumped || block.playing != previous_.playing
            || block.numerator != previous_.numerator || block.denominator != previous_.denominator;
        // When transport sync is off, local playback can continue at the host
        // tempo even while the DAW is stopped. A seek still reanchors its phase.
        block.beat = !hostPlaying && block.playing && !block.discontinuity ? nextLocalBeat_ : *ppq;
        nextLocalBeat_ = block.beat + (block.playing ? frames * block.beatsPerSample : 0.0);
        expectedHostBeat_ = *ppq + (hostPlaying ? frames * block.beatsPerSample : 0.0);
        hostWasPlaying_ = hostPlaying;
        previous_ = block;
        return block;
    }

private:
    Block previous_;
    double expectedHostBeat_ = 0.0;
    double nextLocalBeat_ = 0.0;
    bool hostWasPlaying_ = false;
};

} // namespace ecm
