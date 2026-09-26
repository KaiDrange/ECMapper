#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <functional>

namespace ecm {

// Audio-thread clock phase. MIDI clock is always 24 PPQN, independent of meter.
class MidiClockMaster {
public:
    [[nodiscard]] double quarterNotePosition() const noexcept { return quarterNotePosition_; }
    void process(const int frames, const double sampleRate, const double bpm, const uint64_t transport,
                 const bool enabled, juce::MidiBuffer& output, const bool sendMidi = true) {
        const bool play = enabled && (transport & 1) != 0;
        if (sending_ && (!play || !sendMidi)) output.addEvent(juce::MidiMessage::midiStop(), 0);
        if (play && (!playing_ || transport != transport_)) {
            phase_ = 0;
            quarterNotePosition_ = 0;
            if (sendMidi) output.addEvent(juce::MidiMessage::midiStart(), 0);
        }
        sending_ = play && sendMidi;
        playing_ = play;
        transport_ = transport;
        if (!play) return;
        quarterNotePosition_ += frames * bpm / (60.0 * sampleRate);
        const double increment = bpm * 24.0 / (60.0 * sampleRate);
        for (int frame = 0; frame < frames; ++frame) {
            if (phase_ <= 1.0e-10) {
                if (sendMidi) output.addEvent(juce::MidiMessage::midiClock(), frame);
                phase_ += 1.0;
            }
            phase_ -= increment;
        }
    }
private:
    double quarterNotePosition_ = 0;
    double phase_ = 0;
    uint64_t transport_ = 0;
    bool sending_ = false;
    bool playing_ = false;
};

inline std::array<bool, 3> distinctMidiClockDestinations(const std::array<juce::String, 3>& ids) {
    std::array<bool, 3> result {};
    for (size_t i = 0; i < ids.size(); ++i) {
        result[i] = ids[i].isNotEmpty();
        for (size_t j = 0; j < i; ++j)
            if (ids[j] == ids[i]) result[i] = false;
    }
    return result;
}

// System Real Time messages use UMP message type 1, also on MIDI 2.0 endpoints.
inline uint32_t midiClockSystemPacket(const uint8_t status, const uint8_t group) noexcept {
    return 0x10000000u | (static_cast<uint32_t>(group) << 24)
                       | (static_cast<uint32_t>(status) << 16);
}

// UMP Output has no timestamped-send API. Schedule both protocols on one worker
// so clocks within an audio block are not sent as a burst. One audio producer.
class MidiClockOutput : private juce::Thread {
public:
    explicit MidiClockOutput(std::function<void(uint8_t)> send)
        : Thread("MIDI clock output"), send_(std::move(send)) {}
    ~MidiClockOutput() override { stop(); }
    void start() { startThread(juce::Thread::Priority::high); }
    // Call with the audio producer stopped, before destroying MIDI destinations.
    void stop() {
        stopThread(-1);
        fifo_.reset();
        overflow_.store(false);
        if (playing_) send_(0xfc);
        playing_ = false;
    }
    void schedule(const juce::MidiBuffer& messages, const double blockStartMs, const double sampleRate) noexcept {
        if (!isThreadRunning()) return;
        for (const auto message : messages) {
            const auto write = fifo_.write(1);
            if (write.blockSize1 == 0) {
                overflow_.store(true);
                break;
            }
            events_[static_cast<size_t>(write.startIndex1)] = {
                .dueMs = blockStartMs + message.samplePosition * 1000.0 / sampleRate, .status = message.data[0]
            };
        }
        notify();
    }
private:
    struct Event { double dueMs = 0; uint8_t status = 0; };
    void run() override {
        while (!threadShouldExit()) {
            if (overflow_.exchange(false)) {
                // Fail stopped if the sender cannot keep up; never leave peers running.
                while (fifo_.getNumReady() > 0) { const auto discard = fifo_.read(1); }
                send_(0xfc);
                playing_ = false;
            }
            if (fifo_.getNumReady() == 0) { wait(20); continue; }
            const auto read = fifo_.read(1);
            const auto event = events_[static_cast<size_t>(read.startIndex1)];
            while (!threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < event.dueMs)
                wait(1);
            if (threadShouldExit()) break;
            if (event.status == 0xfa) playing_ = true;
            if (event.status == 0xfc) playing_ = false;
            if (event.status != 0xf8 || playing_) send_(event.status);
        }
    }
    std::function<void(uint8_t)> send_;
    juce::AbstractFifo fifo_ { 1024 };
    std::array<Event, 1024> events_;
    std::atomic<bool> overflow_ { false };
    bool playing_ = false; // Worker only, or after joining it.
};

} // namespace ecm
