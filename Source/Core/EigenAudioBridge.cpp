#include "EigenAudioBridge.h"
#include <cmath>

namespace ecm {

void EigenAudioBridge::prepare(const double hostSampleRate) {
    const juce::ScopedLock lock(preparationLock_);
    ready_.store(false);
    stop(false);
    link_.prepare();
    wasLinkPlaying_ = false;
    clockMaster_ = {};
    hostClock_ = {};
    hostTimingAvailable_.store(false);
    wasRenderingMetronome_ = false;
    inputGain_.reset(std::isfinite(hostSampleRate) && hostSampleRate > 0 ? hostSampleRate : 48000.0, 0.01);
    inputGain_.setCurrentAndTargetValue(inputVolume_.load());
    fifo_.reset();
    resetHeadphoneResampler();
    observedGeneration_ = command_.load();
    dropped_.store(0);
    callbackCount_.store(0);
    playbackPosition_.store(0);
    hostSampleRate_.store(hostSampleRate);
    error_ = metronome_.prepare(hostSampleRate);
    ready_.store(error_.isEmpty());
}

void EigenAudioBridge::resetHeadphoneResampler() noexcept {
    accumulated_ = 0;
    headphoneSamplePhase_ = 0.0;
    previousHeadphoneSample_ = {};
    hasPreviousHeadphoneSample_ = false;
}

void EigenAudioBridge::requestPlayback(const bool play) noexcept {
    auto current = command_.load();
    while (!command_.compare_exchange_weak(current, ((current + 2) & ~static_cast<uint64_t>(1)) | (play ? 1 : 0))) {}
}

void EigenAudioBridge::setHostActive(const bool active) noexcept {
    hostActive_.store(active);
    if (!active && !standaloneClockEnabled_.load()) stop(false);
}

void EigenAudioBridge::setTiming(const double bpm, const int beatsPerBar, const int beatUnit) noexcept {
    metronome_.setTiming(bpm, beatsPerBar, beatUnit);
    quantum_.store(beatsPerBar > 0 ? juce::jlimit(1, 12, beatsPerBar) * 4.0 / (beatUnit == 8 ? 8 : 4) : 4.0);
    if (std::isfinite(bpm) && std::abs(configuredBpm_.exchange(bpm) - bpm) > 1.0e-9)
        link_.requestTempo(bpm);
}

void EigenAudioBridge::setLinkEnabled(const bool enabled) {
    if (link_.isEnabled() == enabled) return;
    stop(false);
    link_.setStartStopSync(startStopSync_.load());
    link_.setEnabled(enabled);
}

void EigenAudioBridge::setAudioOutputEnabled(const bool enabled) noexcept {
    auto state = audioOutputState_.load();
    while ((state & 1) != static_cast<uint64_t>(enabled)
           && !audioOutputState_.compare_exchange_weak(state, ((state + 2) & ~static_cast<uint64_t>(1)) | (enabled ? 1 : 0))) {}
}

void EigenAudioBridge::setRouting(int metronomeRoute, int inputRoute) noexcept {
    const auto routes = static_cast<uint64_t>((juce::jlimit(1, 4, metronomeRoute) & 3)
                                              | ((juce::jlimit(1, 4, inputRoute) & 3) << 2));
    auto current = routingState_.load();
    while ((current & 15) != routes
           && !routingState_.compare_exchange_weak(current, ((current + 16) & ~uint64_t(15)) | routes)) {}
}

juce::String EigenAudioBridge::start() {
    const juce::ScopedLock lock(preparationLock_);
    if (midiSlave_.load() && startStopSync_.load()) return "Transport follows the selected MIDI input in slave mode.";
    if (hostSyncEnabled_.load() && startStopSync_.load()) return "Use the host application's transport to start playback.";
    if (isNonRealtime()) return "Eigenharp playback is disabled during offline rendering.";
    if (standaloneClockEnabled_.load()) {
        const double rate = hostSampleRate_.load();
        if (!std::isfinite(rate) || rate <= 0.0) return "Select an audio device to run the MIDI clock.";
    } else {
        if (!ready_.load()) return error_;
        if (!hostActive_.load() && !metronomeUsesDeviceOutput() && !link_.isEnabled())
            return "The metronome is available in Host mode only.";
    }
    requestPlayback(true);
    if (link_.isEnabled()) link_.requestStart();
    return {};
}

void EigenAudioBridge::stop(const bool shareWithLink) noexcept {
    midiPlaying_.store(false);
    hostPlaying_.store(false);
    requestPlayback(false);
    if (shareWithLink && link_.isEnabled()) link_.requestStop();
    else link_.resetPlayback();
}

void EigenAudioBridge::setMidiSlave(const bool enabled) noexcept {
    if (midiSlave_.exchange(enabled) != enabled) stop(false);
}

bool EigenAudioBridge::isPlaying() const noexcept {
    const auto command = command_.load();
    return !isNonRealtime() && (standaloneClockEnabled_.load() || hostActive_.load() || metronomeUsesDeviceOutput() || link_.isEnabled())
        && (hostSyncEnabled_.load() ? hostPlaying_.load()
            : link_.isEnabled() ? link_.isPlaying() : (midiSlave_.load() ? midiPlaying_.load() : (command & 1) != 0));
}

void EigenAudioBridge::process(const int numFrames, const bool nonRealtime, const juce::MidiBuffer* midi, juce::MidiBuffer* clockOutput, juce::AudioBuffer<float>* audio, int inputChannels, int outputChannels, const juce::AudioPlayHead::PositionInfo* hostPosition) noexcept {
    // Producer only. Never reset the shared FIFO while its consumer is active.
    if (nonRealtime != isNonRealtime()) {
        transportState_.fetch_add(1);
        resetHeadphoneResampler();
        hostClock_ = {};
        hostTimingAvailable_.store(false);
        if (nonRealtime) stop(false);
        else link_.prepare();
    }
    auto command = command_.load();
    const double quarterNotePosition = command != observedGeneration_ ? 0.0 : clockMaster_.quarterNotePosition();
    const double rate = hostSampleRate_.load();
    if (clockOutput != nullptr)
        clockMaster_.process(numFrames, rate, metronome_.bpm(), command,
                             !nonRealtime && std::isfinite(rate) && rate > 0.0
                                 && (standaloneClockEnabled_.load() || (ready_.load() && hostActive_.load()))
                                 && !midiSlave_.load() && !link_.isEnabled() && !hostSyncEnabled_.load(), *clockOutput, midiClockOutputEnabled_.load(), startStopSync_.load());
    if (nonRealtime) { if (audio != nullptr) audio->clear(); return; }
    const bool linked = link_.isEnabled();
    const auto linkBlock = link_.process(numFrames, rate, quantum_.load(),
                                         metronomeUsesDeviceOutput() ? outputLatency_.load() : 0.0);
    if (linked && linkBlock.playing != wasLinkPlaying_) {
        // Remote transport transitions also invalidate already queued USB audio.
        requestPlayback(linkBlock.playing);
        command = command_.load();
        metronome_.reset();
        resetHeadphoneResampler();
    }
    wasLinkPlaying_ = linked && linkBlock.playing;
    const bool hostSynced = hostSyncEnabled_.load();
    const auto hostBlock = hostClock_.process(hostSynced ? hostPosition : nullptr, numFrames, rate,
                                             startStopSync_.load(), (command & 1) != 0);
    hostTimingAvailable_.store(hostSynced && hostBlock.valid);
    hostPlaying_.store(hostSynced && hostBlock.playing);
    if (hostSynced && hostBlock.valid) {
        hostTempo_.store(hostBlock.bpm);
        hostNumerator_.store(hostBlock.numerator);
        hostDenominator_.store(hostBlock.denominator);
    }
    if (hostSynced && hostBlock.discontinuity) {
        // Host seeks/loops/transport changes must discard queued headphone audio
        // and old click tails, even when the DAW remains in the playing state.
        requestPlayback(startStopSync_.load() ? hostBlock.playing : (command & 1) != 0);
        command = command_.load();
    }
    callbackCount_.fetch_add(1, std::memory_order_relaxed);
    const auto audioOutputState = audioOutputState_.load();
    const auto routing = routingState_.load();
    const int clickRoute = static_cast<int>(routing & 3);
    const int inputRoute = static_cast<int>((routing >> 2) & 3);
    const bool headphones = hostActive_.load() && (audioOutputState & 1) != 0 && ready_.load();
    inputChannels = audio != nullptr ? juce::jlimit(0, audio->getNumChannels(), inputChannels) : 0;
    outputChannels = audio != nullptr ? juce::jlimit(0, audio->getNumChannels(), outputChannels) : 0;
    const bool renderClick = ready_.load() && (((clickRoute & 1) != 0 && headphones)
                                               || ((clickRoute & 2) != 0 && outputChannels > 0));
    if (!headphones || audioOutputState != observedAudioOutputState_ || routing != observedRoutingState_)
        resetHeadphoneResampler();
    observedAudioOutputState_ = audioOutputState;
    observedRoutingState_ = routing;
    if (command != observedGeneration_) {
        resetHeadphoneResampler();
        metronome_.reset();
        if (midiSlave_.load() && !startStopSync_.load() && (command & 1) != 0)
            metronome_.handleMidiClock(juce::MidiMessage::midiStart());
    }
    observedGeneration_ = command;
    const bool slave = midiSlave_.load();
    if (renderClick) {
        metronome_.beginBlock();
        if (!wasRenderingMetronome_) {
            if (!slave && !linked && !hostSynced && clockOutput != nullptr) metronome_.seekQuarterNote(quarterNotePosition);
            else {
                metronome_.reset();
                if (slave && !startStopSync_.load() && (command & 1) != 0)
                    metronome_.handleMidiClock(juce::MidiMessage::midiStart());
            }
        }
    }
    wasRenderingMetronome_ = renderClick;
    inputGain_.setTargetValue(inputVolume_.load());
    if (!renderClick && !headphones && audio == nullptr) { midiPlaying_.store(false); return; }
    auto event = midi != nullptr ? midi->begin() : juce::MidiBufferIterator{};
    const auto end = midi != nullptr ? midi->end() : juce::MidiBufferIterator{};
    for (int frame = 0; frame < numFrames; ++frame) {
        if (slave && renderClick) {
            while (event != end && (*event).samplePosition <= frame) {
                const auto message = *event;
                // Only short system messages are relevant; avoid copying/allocating SysEx.
                if (message.numBytes > 0 && message.numBytes <= 3 && message.data[0] >= 0xf2
                    && (startStopSync_.load() || message.data[0] == 0xf8))
                    metronome_.handleMidiClock(message.getMessage());
                ++event;
            }
        }
        const float click = !renderClick ? 0.0f
            : hostSynced ? metronome_.nextHostSample(hostBlock.beat + frame * hostBlock.beatsPerSample,
                                                     hostBlock.beatsPerSample, hostBlock.barStart,
                                                     hostBlock.numerator, hostBlock.denominator, hostBlock.playing)
            : linked ? metronome_.nextLinkSample(linkBlock.beat + frame * linkBlock.beatsPerSample,
                                                 linkBlock.beatsPerSample, linkBlock.playing)
            : slave ? metronome_.nextMidiSample() : metronome_.nextSample((command & 1) != 0);
        const float gain = inputGain_.getNextValue();
        const float left = inputChannels > 0 ? audio->getSample(0, frame) * gain : 0.0f;
        const float right = inputChannels > 1 ? audio->getSample(1, frame) * gain : left;
        if (audio != nullptr) {
            for (int channel = 0; channel < audio->getNumChannels(); ++channel) {
                float output = 0.0f;
                if (channel < outputChannels) {
                    if ((inputRoute & 2) != 0)
                        output = outputChannels == 1 ? (left + right) * 0.5f : (channel == 0 ? left : (channel == 1 ? right : 0.0f));
                    if ((clickRoute & 2) != 0 && channel < 2) output += click;
                }
                audio->setSample(channel, frame, output);
            }
        }
        if (!headphones) continue;
        const float headphoneClick = (clickRoute & 1) != 0 ? click : 0.0f;
        const std::array<float, 2> current {
            headphoneClick + ((inputRoute & 1) != 0 ? left : 0.0f),
            headphoneClick + ((inputRoute & 1) != 0 ? right : 0.0f)
        };
        if (!hasPreviousHeadphoneSample_) {
            previousHeadphoneSample_ = current;
            hasPreviousHeadphoneSample_ = true;
        }
        // Linear interpolation of the final stereo mix. Fractional phase and the
        // previous sample survive callback boundaries. No buffers or allocations.
        // Waiting for the right-hand sample costs at most one host sample.
        while (headphoneSamplePhase_ <= 1.0e-9) {
            const float fraction = static_cast<float>(juce::jlimit(0.0, 1.0, headphoneSamplePhase_ + 1.0));
            for (size_t channel = 0; channel < 2; ++channel) {
                // Preserve the original samples exactly at 48 kHz (and aligned positions).
                const float sample = headphoneSamplePhase_ >= -1.0e-9 ? current[channel]
                    : previousHeadphoneSample_[channel] + fraction * (current[channel] - previousHeadphoneSample_[channel]);
                accumulator_.stereo[static_cast<size_t>(accumulated_ * 2) + channel] = sample;
            }
            if (++accumulated_ == blockFrames) {
                accumulator_.routingState = routing;
                accumulator_.audioOutputState = audioOutputState;
                accumulator_.generation = command;
                accumulator_.transportState = transportState();
                const auto write = fifo_.write(1);
                if (write.blockSize1 != 0) queue_[static_cast<std::size_t>(write.startIndex1)] = accumulator_;
                else dropped_.fetch_add(1);
                accumulated_ = 0;
            }
            headphoneSamplePhase_ += rate / 48000.0;
        }
        headphoneSamplePhase_ -= 1.0;
        previousHeadphoneSample_ = current;
    }
    midiPlaying_.store(renderClick && slave && metronome_.midiPlaying());
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
        if (queued.generation == command_.load() && queued.routingState == routingState_.load() && hostActive_.load()
            && (audioOutputState_.load() & 1) != 0 && queued.audioOutputState == audioOutputState_.load()
            && !isNonRealtime() && queued.transportState == transportState()) {
            block = queued;
            return true;
        }
    }
    return false;
}

} // namespace ecm
