#include "Core/LinkClock.h"
#include "Core/Metronome.h"
#include <ableton/Link.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>

int main(int argc, char** argv) {
    bool ok = true;
    const auto expect = [&](const bool condition, const char* message) {
        if (!condition) { std::cerr << message << '\n'; ok = false; }
    };
    // Compare every rendered sample against the existing metronome, including
    // eighth-note meters, count-in silence, irregular callbacks and click tails.
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        for (const int denominator : {4, 8}) {
            for (const int numerator : {0, 3, 4, 7}) {
                ecm::Metronome reference, linked;
                expect(reference.prepare(rate).isEmpty() && linked.prepare(rate).isEmpty(), "Clicks must prepare");
                reference.setTiming(137.3, numerator, denominator);
                linked.setTiming(137.3, numerator, denominator);
                reference.beginBlock();
                linked.beginBlock();
                const double increment = 137.3 / (60.0 * rate);
                for (int i = -500; i < 0; ++i)
                    expect(linked.nextLinkSample(i * increment, increment, true) == 0.0f, "Quantized count-in must be silent");
                for (int sample = 0; sample < static_cast<int>(rate * 4); ++sample) {
                    if (sample % 97 == 0 || sample % 256 == 0) {
                        reference.beginBlock();
                        linked.beginBlock();
                    }
                    if (std::abs(reference.nextSample(true) - linked.nextLinkSample(sample * increment, increment, true)) > 1.0e-6f) {
                        expect(false, "Link phase, meter accent, or callback-spanning click tail differs from reference");
                        return 1;
                    }
                }
                expect(linked.nextLinkSample(10.0, increment, false) == 0.0f, "Stop must silence a click immediately");
                linked.reset();
                linked.beginBlock();
                for (int sample = 0; sample < 100; ++sample)
                    expect(linked.nextLinkSample(0.25 + sample * increment, increment, true) == 0.0f,
                           "Enabling output mid-beat must wait for the next boundary");
            }
        }
    }

    ecm::Metronome corrected;
    expect(corrected.prepare(48000).isEmpty(), "Clock-correction test must prepare");
    corrected.beginBlock();
    corrected.nextLinkSample(-0.001, 1.0 / 24000.0, true);
    corrected.nextLinkSample(0.001, 1.0 / 24000.0, true);
    expect(corrected.samplePosition() == 1, "A callback clock correction across beat zero must not lose the first click");
    corrected.nextLinkSample(0.99995, 1.0 / 24000.0, true);
    corrected.nextLinkSample(1.00001, 1.0 / 24000.0, true);
    corrected.nextLinkSample(0.99999, 1.0 / 24000.0, true);
    corrected.nextLinkSample(1.00002, 1.0 / 24000.0, true);
    expect(corrected.samplePosition() == 3, "A backwards clock correction must not trigger the same beat twice");

    ecm::LinkClock clock;
    expect(!clock.isEnabled() && !clock.isPlaying() && clock.peers() == 0, "Link must be opt-in and stopped");
    clock.requestStart();
    expect(!clock.processAtTime(48000, 4, 1000000).playing, "Disabled Link cannot start playback");
    expect(!clock.processAtTime(0, 4, 1000000).playing, "Invalid audio rates must be rejected");
    clock.requestTempo(137.3);
    clock.processAtTime(48000, 4, 1000000);
    clock.requestTempo(std::numeric_limits<double>::quiet_NaN());
    const auto block = clock.processAtTime(48000, 4, 1500000);
    expect(std::abs(block.beatsPerSample - 137.3 / (60.0 * 48000)) < 1.0e-9, "Tempo edits must reach the Link timeline");

    // Optional live integration check. Run on an otherwise empty Link network.
    if (argc > 1 && std::string(argv[1]) == "--network") {
        using namespace std::chrono_literals;
        ableton::Link peer(151.0);
        peer.enableStartStopSync(true);
        peer.enable(true);
        // Link treats sessions created within 500 ms as simultaneous and breaks
        // ties by session ID. Establish the existing session before joining it.
        std::this_thread::sleep_for(750ms);
        clock.setStartStopSync(true);
        clock.setEnabled(true);
        ecm::LinkClock::Block current;
        const auto pump = [&] {
            current = clock.processAtTime(48000, 4, peer.clock().micros().count());
            std::this_thread::sleep_for(5ms);
        };
        const auto waitFor = [&](auto predicate) {
            for (int i = 0; i < 1000; ++i) { pump(); if (predicate()) return true; }
            return false;
        };
        expect(waitFor([&] { return clock.peers() > 0 && peer.numPeers() > 0; }), "Two Link participants must discover each other");
        if (clock.peers() != 1 || peer.numPeers() != 1) {
            std::cerr << "Network test requires exactly two participants; stopping before session edits\n";
            return 1;
        }
        expect(waitFor([&] { return std::abs(clock.tempo() - 151.0) < 0.01; }), "Joining must adopt the peer tempo without overwriting it");
        expect(!current.playing, "Joining a session must not start local playback");
        clock.requestStart();
        pump();
        expect(current.playing && current.beat <= 0.05 && current.beat > -4.01, "Start must quantize to the next bar");
        expect(waitFor([&] { return peer.captureAppSessionState().isPlaying(); }), "Local Start must reach a synchronized peer");
        clock.requestTempo(173.0);
        expect(waitFor([&] { return std::abs(peer.captureAppSessionState().tempo() - 173.0) < 0.01; }), "Local tempo must reach the peer");
        auto session = peer.captureAppSessionState();
        session.setTempo(89.0, peer.clock().micros());
        peer.commitAppSessionState(session);
        expect(waitFor([&] { return std::abs(clock.tempo() - 89.0) < 0.01; }), "Remote tempo must reach ECMapper");
        session = peer.captureAppSessionState();
        session.setIsPlaying(false, peer.clock().micros());
        peer.commitAppSessionState(session);
        expect(waitFor([&] { return !current.playing; }), "Remote Stop must stop synchronized playback");
        session = peer.captureAppSessionState();
        session.setIsPlaying(true, peer.clock().micros());
        peer.commitAppSessionState(session);
        expect(waitFor([&] { return current.playing; }), "Remote Start must start synchronized playback");
        const auto now = peer.clock().micros();
        current = clock.processAtTime(48000, 4, now.count());
        const auto peerPhase = peer.captureAppSessionState().phaseAtTime(now, 4);
        expect(std::abs(std::remainder(current.beat - peerPhase, 4.0)) < 0.02, "Both participants must share bar phase");
        clock.resetPlayback();
        pump();
        expect(!current.playing && peer.captureAppSessionState().isPlaying(), "Lifecycle reset must stop locally without stopping peers");
        clock.setStartStopSync(false);
        clock.requestStart();
        pump();
        session = peer.captureAppSessionState();
        session.setIsPlaying(false, peer.clock().micros());
        peer.commitAppSessionState(session);
        for (int i = 0; i < 100; ++i) pump();
        expect(current.playing, "Remote Stop must be ignored when start/stop sync is off");
        clock.requestStop();
        pump();
        expect(!current.playing, "Local Stop must work with transport sync disabled");
        clock.setEnabled(false);
        pump();
        expect(!clock.isPlaying(), "Disabling Link must stop local Link playback");
    }
    if (ok) std::cout << "Link clock checks passed\n";
    return ok ? 0 : 1;
}
