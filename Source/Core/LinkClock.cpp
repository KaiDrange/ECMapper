#include "LinkClock.h"
#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace ecm {

struct LinkClock::Impl {
    ableton::Link link { 120.0 };
    ableton::link::HostTimeFilter<ableton::Link::Clock> hostTime;
    std::atomic<double> requestedTempo { 0.0 };
    std::atomic<int> request { 0 }; // 1=start, 2=stop, 3=local lifecycle reset.
    std::atomic<bool> playing { false };
    bool initialized = false;
    bool localPlaying = false;
    std::chrono::microseconds lastTransportTime {};
    double sampleTime = 0.0;
};

LinkClock::LinkClock() : impl_(std::make_unique<Impl>()) {}
LinkClock::~LinkClock() = default;

void LinkClock::setEnabled(const bool enabled) {
    if (enabled == isEnabled()) return;
    resetPlayback();
    // A saved/local tempo must not overwrite the session we are joining.
    impl_->requestedTempo.store(0.0);
    impl_->link.enable(enabled);
}

bool LinkClock::isEnabled() const noexcept { return impl_->link.isEnabled(); }
void LinkClock::setStartStopSync(const bool enabled) noexcept { impl_->link.enableStartStopSync(enabled); }
void LinkClock::requestTempo(const double bpm) noexcept {
    if (std::isfinite(bpm)) impl_->requestedTempo.store(std::clamp(bpm, 20.0, 999.0));
}
void LinkClock::requestStart() noexcept { impl_->request.store(1); }
void LinkClock::requestStop() noexcept { impl_->request.store(2); impl_->playing.store(false); }
void LinkClock::resetPlayback() noexcept { impl_->request.store(3); impl_->playing.store(false); }
double LinkClock::tempo() const { return impl_->link.captureAppSessionState().tempo(); }
int LinkClock::peers() const noexcept { return static_cast<int>(impl_->link.numPeers()); }
bool LinkClock::isPlaying() const noexcept { return impl_->playing.load(); }

void LinkClock::prepare() noexcept {
    impl_->hostTime.reset();
    impl_->sampleTime = 0.0;
    impl_->initialized = false;
    resetPlayback();
}

LinkClock::Block LinkClock::process(const int frames, const double sampleRate,
                                  const double quantum, const double outputLatencySeconds) noexcept {
    const auto time = impl_->hostTime.sampleTimeToHostTime(impl_->sampleTime);
    impl_->sampleTime += frames;
    return processAtTime(sampleRate, quantum,
                         time.count() + static_cast<int64_t>(std::llround(outputLatencySeconds * 1.0e6)));
}

LinkClock::Block LinkClock::processAtTime(const double sampleRate, const double quantum,
                                        const int64_t outputTimeMicros) noexcept {
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0 || !std::isfinite(quantum) || quantum <= 0.0)
        return {};
    auto& p = *impl_;
    auto session = p.link.captureAudioSessionState();
    const auto time = std::chrono::microseconds(outputTimeMicros);
    const auto request = p.request.exchange(0);
    const bool enabled = isEnabled();
    if (!p.initialized || request == 3 || !enabled) {
        p.localPlaying = false;
        p.lastTransportTime = session.timeForIsPlaying();
        p.initialized = true;
    }
    const bool wasPlaying = p.localPlaying;
    if (enabled && p.link.isStartStopSyncEnabled() && session.timeForIsPlaying() != p.lastTransportTime)
        p.localPlaying = session.isPlaying();
    if (enabled && (request == 1 || request == 2)) {
        p.localPlaying = request == 1;
        session.setIsPlaying(p.localPlaying, time);
    }
    if (p.localPlaying && !wasPlaying)
        session.requestBeatAtTime(0.0, request == 1 ? time : session.timeForIsPlaying(), quantum);
    if (const auto bpm = p.requestedTempo.exchange(0.0); bpm > 0.0)
        session.setTempo(bpm, time);
    p.link.commitAudioSessionState(session);
    p.lastTransportTime = session.timeForIsPlaying();
    p.playing.store(enabled && p.localPlaying);
    const auto beat = session.beatAtTime(time, quantum);
    // Link's beat rate includes clock-drift compensation, unlike tempo()/60.
    const auto beatsPerSecond = session.beatAtTime(time + std::chrono::seconds(1), quantum) - beat;
    return { enabled && p.localPlaying, beat, beatsPerSecond / sampleRate };
}

} // namespace ecm
