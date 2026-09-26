#include "Core/EigenAudioBridge.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <BinaryData.h>
#include <cmath>
#include <iostream>

int main() {
    bool ok = true;
    auto expect = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << message << '\n'; ok = false; }
    };
    juce::AudioBuffer<float> clicks[2];
    const char* data[] = { BinaryData::click1_wav, BinaryData::click2_wav };
    const int sizes[] = { BinaryData::click1_wavSize, BinaryData::click2_wavSize };
    for (int i = 0; i < 2; ++i) {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(
            new juce::MemoryInputStream(data[i], static_cast<size_t>(sizes[i]), false), true));
        if (!reader || reader->sampleRate != 48000 || reader->numChannels != 1) return 1;
        clicks[i].setSize(1, static_cast<int>(reader->lengthInSamples));
        reader->read(&clicks[i], 0, clicks[i].getNumSamples(), 0, true, false);
    }
    ecm::EigenAudioBridge bridge;
    bridge.setAudioOutputEnabled(true);
    ecm::EigenAudioBridge::Block block;
    bridge.prepare(48000);
    expect(bridge.start().isNotEmpty(), "Inactive bridge must reject Start");
    bridge.setHostActive(true);
    // Fractional beat lengths exercise accumulated timing across callback boundaries.
    for (const auto bpm : {120.0, 137.3}) {
        for (const int numerator : {0, 3, 4, 6}) {
            for (const int denominator : {4, 8}) {
                bridge.setTiming(bpm, numerator, denominator);
                expect(bridge.start().isEmpty(), "Metronome should start");
                const double interval = 48000.0 * 240.0 / (bpm * denominator);
                int beat = 0;
                int onset = 0;
                for (int offset = 0; offset < static_cast<int>(interval * 15); offset += 128) {
                    bridge.process(31);
                    bridge.process(97);
                    expect(bridge.pop(block), "Callback fragments must produce a block");
                    for (int i = 0; i < 128; ++i) {
                        const int frame = offset + i;
                        if (frame >= static_cast<int>(std::ceil((beat + 1) * interval - 1e-7))) {
                            ++beat;
                            onset = static_cast<int>(std::ceil(beat * interval - 1e-7));
                        }
                        const auto& click = clicks[numerator > 0 && beat % numerator == 0 ? 0 : 1];
                        const int position = frame - onset;
                        const float expected = position < click.getNumSamples() ? click.getSample(0, position) : 0.0f;
                        if (std::abs(block.stereo[static_cast<size_t>(i * 2)] - expected) > 1e-6f
                            || block.stereo[static_cast<size_t>(i * 2)] != block.stereo[static_cast<size_t>(i * 2 + 1)]) {
                            expect(false, "Click timing, accent, or mono duplication mismatch");
                            return 1;
                        }
                    }
                }
                expect(bridge.isPlaying(), "Metronome must continue beyond the click sample");
            }
        }
    }
    bridge.process(128);
    bridge.stop();
    expect(!bridge.pop(block), "Stop must invalidate queued audio");
    bridge.process(128);
    expect(bridge.pop(block) && std::all_of(block.stereo.begin(), block.stereo.end(), [](float x) { return x == 0; }),
           "Stopped transport must send silence");
    bridge.start();
    bridge.process(63);
    bridge.stop();
    bridge.start();
    bridge.setTiming(120, 4, 4);
    bridge.process(128);
    expect(bridge.pop(block), "Restart must discard partial block");
    for (int i = 0; i < 128; ++i)
        expect(block.stereo[static_cast<size_t>(2 * i)] == clicks[0].getSample(0, i), "Restart must begin with accent");
    bridge.process(128);
    bridge.process(8192, true);
    expect(!bridge.isPlaying() && !bridge.pop(block) && bridge.start().isNotEmpty(), "Offline rendering must suppress playback");
    bridge.process(128, false);
    expect(bridge.pop(block) && !bridge.isPlaying(), "Realtime resume must remain stopped");
    bridge.start();
    bridge.process(128 * (ecm::EigenAudioBridge::queueBlocks + 2));
    expect(bridge.droppedBlocks() == 3, "Overflow must remain bounded");
    bridge.setHostActive(false);
    expect(!bridge.pop(block) && !bridge.isPlaying(), "Leaving Host mode must discard audio");
    bridge.setVolume(0.5f);
    bridge.prepare(48000);
    bridge.setHostActive(true);
    bridge.start();
    bridge.process(128);
    expect(bridge.pop(block), "Volume test must produce block");
    for (int i = 0; i < 128; ++i)
        expect(block.stereo[static_cast<size_t>(2 * i)] == clicks[0].getSample(0, i) * 0.5f, "Volume must scale clicks");
    // Slave clicks are pulse-driven, regardless of the configured local BPM.
    bridge.prepare(48000);
    bridge.setVolume(1.0f);
    bridge.prepare(48000);
    bridge.setMidiSlave(true);
    bridge.setTiming(37, 3, 8);
    expect(bridge.start().isNotEmpty(), "Slave transport must be controlled by MIDI");
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::midiStart(), 7);
    midi.addEvent(juce::MidiMessage::midiClock(), 31);
    bridge.process(128, false, &midi);
    expect(bridge.pop(block) && bridge.isPlaying(), "MIDI Start and clock must start slave");
    for (int i = 0; i < 128; ++i)
        expect(block.stereo[static_cast<size_t>(i * 2)] == (i < 31 ? 0.0f : clicks[0].getSample(0, i - 31)),
               "First slave click must respect clock sample offset, not Start offset");
    midi.clear();
    midi.addEvent(juce::MidiMessage::midiStop(), 19);
    bridge.process(128, false, &midi);
    expect(bridge.pop(block) && !bridge.isPlaying(), "MIDI Stop must stop slave");
    for (int i = 19; i < 128; ++i)
        expect(block.stereo[static_cast<size_t>(i * 2)] == 0, "Stop must silence at its sample offset");
    midi.clear();
    midi.addEvent(juce::MidiMessage::songPositionPointer(2), 0); // Second eighth note.
    midi.addEvent(juce::MidiMessage::midiContinue(), 0);
    midi.addEvent(juce::MidiMessage::midiClock(), 13);
    bridge.process(128, false, &midi);
    expect(bridge.pop(block), "Continue must produce a block");
    for (int i = 0; i < 128; ++i)
        expect(block.stereo[static_cast<size_t>(i * 2)] == (i < 13 ? 0.0f : clicks[1].getSample(0, i - 13)),
               "Song position and Continue must preserve meter accent phase");
    // Exercise all beat boundaries and accents, including clocks while stopped.
    for (const int denominator : {4, 8}) {
        ecm::Metronome slave;
        expect(slave.prepare(48000).isEmpty(), "Slave metronome must prepare");
        slave.setTiming(300, 3, denominator);
        slave.beginBlock();
        slave.handleMidiClock(juce::MidiMessage::midiClock());
        expect(slave.nextMidiSample() == 0 && !slave.midiPlaying(), "Clock alone must not start playback");
        slave.handleMidiClock(juce::MidiMessage::midiStart());
        const int clocksPerBeat = 96 / denominator;
        int onset = 0;
        int clickIndex = 0;
        for (int pulse = 0; pulse < clocksPerBeat * 9; ++pulse) {
            slave.handleMidiClock(juce::MidiMessage::midiClock());
            if (pulse % clocksPerBeat == 0) {
                onset = pulse * 1000;
                clickIndex = (pulse / clocksPerBeat) % 3 == 0 ? 0 : 1;
            }
            for (int i = 0; i < 1000; ++i) {
                const int sample = pulse * 1000 + i - onset;
                const float expected = sample < clicks[clickIndex].getNumSamples() ? clicks[clickIndex].getSample(0, sample) : 0.0f;
                expect(slave.nextMidiSample() == expected, "Slave pulse spacing or meter accent mismatch");
            }
        }
        slave.handleMidiClock(juce::MidiMessage::midiStop());
        for (int i = 0; i < 7; ++i) slave.handleMidiClock(juce::MidiMessage::midiClock());
        slave.handleMidiClock(juce::MidiMessage::midiContinue());
        slave.handleMidiClock(juce::MidiMessage::midiClock());
        for (int i = 0; i < 128; ++i)
            expect(slave.nextMidiSample() == clicks[0].getSample(0, i), "Continue must retain position through stopped clocks");
        for (int i = 0; i < 96000; ++i) slave.nextMidiSample();
        expect(!slave.midiPlaying() && slave.nextMidiSample() == 0, "Clock loss must stop and silence slave");
    }
    bridge.process(128, true, &midi);
    expect(!bridge.isPlaying() && !bridge.pop(block), "Offline mode must suppress MIDI slave");
    midi.clear();
    bridge.process(128, false, &midi);
    expect(!bridge.isPlaying(), "Realtime resume must wait for MIDI transport");
    bridge.setMidiSlave(false);
    expect(bridge.start().isEmpty(), "Switching back to master must restore manual transport");
    bridge.setHostActive(false);
    bridge.prepare(44100);
    bridge.setHostActive(true);
    expect(bridge.start().isNotEmpty(), "Unsupported sample rate must be rejected");
    bridge.prepare(48000);
    bridge.setTiming(120, 6, 8);
    bridge.setHostActive(true);
    bridge.start();
    juce::MidiBuffer masterEvents;
    bridge.process(2048, false, nullptr, &masterEvents);
    expect(masterEvents.getNumEvents() == 4, "Bridge must emit Start and three 120 BPM clock pulses in 2048 frames");
    masterEvents.clear();
    bridge.setMidiSlave(true);
    bridge.process(128, false, nullptr, &masterEvents);
    expect(masterEvents.getNumEvents() == 1 && (*masterEvents.begin()).getMessage().isMidiStop(),
           "Switching to slave must stop outgoing master transport");
    masterEvents.clear();
    midi.clear();
    midi.addEvent(juce::MidiMessage::midiStart(), 0);
    midi.addEvent(juce::MidiMessage::midiClock(), 0);
    bridge.process(128, false, &midi, &masterEvents);
    expect(masterEvents.isEmpty(), "Slave mode must not generate an outgoing master clock");
    bridge.setMidiSlave(false);
    bridge.start();
    bridge.process(128, false, nullptr, &masterEvents);
    masterEvents.clear();
    bridge.process(128, true, nullptr, &masterEvents);
    expect(masterEvents.getNumEvents() == 1 && (*masterEvents.begin()).getMessage().isMidiStop(),
           "Entering offline mode must send Stop instead of offline clock pulses");
    // A standalone MIDI master requires audio callbacks, not Eigenharp hardware or 48 kHz.
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        ecm::EigenAudioBridge clockOnly;
        clockOnly.setStandaloneClockEnabled(true);
        expect(clockOnly.start().isNotEmpty(), "Clock must reject Start before audio preparation");
        clockOnly.prepare(sampleRate);
        clockOnly.setTiming(120, 4, 4);
        expect(clockOnly.start().isEmpty() && clockOnly.isPlaying(), "Standalone clock must start without local hardware");
        juce::MidiBuffer clockEvents;
        int pulses = 0;
        int starts = 0;
        const int framesPerSecond = static_cast<int>(sampleRate);
        for (int offset = 0; offset < framesPerSecond;) {
            const int frames = juce::jmin(256, framesPerSecond - offset);
            clockEvents.clear();
            clockOnly.process(frames, false, nullptr, &clockEvents);
            for (auto event : clockEvents) {
                if (event.getMessage().isMidiClock()) ++pulses;
                if (event.getMessage().isMidiStart()) ++starts;
            }
            expect(!clockOnly.pop(block), "Clock-only mode must not queue headphone audio");
            offset += frames;
        }
        expect(pulses == 48 && starts == 1, "Clock-only output must keep 24 PPQN at every audio sample rate");
        expect(clockOnly.droppedBlocks() == 0, "Clock-only mode must not fill an unconsumed audio FIFO");
        clockOnly.stop();
        clockEvents.clear();
        clockOnly.process(128, false, nullptr, &clockEvents);
        expect(clockEvents.getNumEvents() == 1 && (*clockEvents.begin()).getMessage().isMidiStop(),
               "Clock-only Stop must reach MIDI outputs");
    }
    // Enabling headphones partway through a beat must not restart the MIDI clock or click off-beat.
    ecm::EigenAudioBridge hotplug;
    hotplug.setStandaloneClockEnabled(true);
    hotplug.prepare(48000);
    hotplug.setTiming(120, 4, 4);
    hotplug.start();
    juce::MidiBuffer hotplugEvents;
    hotplug.process(12000, false, nullptr, &hotplugEvents);
    hotplug.setHostActive(true);
    hotplug.setAudioOutputEnabled(true);
    hotplugEvents.clear();
    for (int frame = 12000; frame < 24128; frame += 16) {
        hotplug.process(16, false, nullptr, &hotplugEvents);
        if ((frame - 12000 + 16) % 128 == 0) {
            expect(hotplug.pop(block), "Enabled headphones must receive audio blocks");
            const int blockStart = frame + 16 - 128;
            for (int sample = 0; sample < 128; ++sample) {
                const int position = blockStart + sample - 24000;
                const float expected = position >= 0 ? clicks[1].getSample(0, position) : 0.0f;
                expect(block.stereo[static_cast<size_t>(sample * 2)] == expected,
                       "Headphone audio enabled mid-beat must join the running clock phase");
            }
        }
    }
    for (auto event : hotplugEvents)
        expect(event.getMessage().isMidiClock(), "Enabling headphones must not restart MIDI transport");
    hotplug.setAudioOutputEnabled(false);
    hotplug.setHostActive(false);
    hotplugEvents.clear();
    hotplug.process(1024, false, nullptr, &hotplugEvents);
    expect(hotplug.isPlaying() && !hotplugEvents.isEmpty() && !hotplug.pop(block),
           "Removing the last headphone output must preserve clock and discard queued audio");
    ecm::EigenAudioBridge localClick;
    localClick.prepare(48000);
    localClick.setStandaloneClockEnabled(true);
    localClick.setHostActive(true);
    localClick.setAudioOutputEnabled(true);
    localClick.setMidiClockOutputEnabled(false);
    localClick.setTiming(120, 4, 4);
    expect(localClick.start().isEmpty(), "Metronome-only mode must support manual Start");
    juce::MidiBuffer localClockEvents;
    localClick.process(128, false, nullptr, &localClockEvents);
    expect(localClockEvents.isEmpty() && localClick.pop(block), "Metronome-only mode must render audio without sending MIDI");
    for (int i = 0; i < 128; ++i)
        expect(std::abs(block.stereo[static_cast<size_t>(i * 2)] - clicks[0].getSample(0, i)) < 1e-7f,
               "Metronome-only mode must retain the local accented click");
    localClick.stop();
    localClick.process(128, false, nullptr, &localClockEvents);
    expect(localClockEvents.isEmpty(), "Metronome-only Stop must not send MIDI transport");
    localClick.setMidiClockOutputEnabled(true);
    localClick.start();
    localClick.process(128, false, nullptr, &localClockEvents);
    expect(localClockEvents.getNumEvents() == 2, "Switching to master must restore Start and clock");
    localClockEvents.clear();
    localClick.setMidiClockOutputEnabled(false);
    localClick.process(128, false, nullptr, &localClockEvents);
    expect(localClockEvents.getNumEvents() == 1 && (*localClockEvents.begin()).getMessage().isMidiStop(),
           "Leaving an active master must send one final Stop to its receivers");
    localClockEvents.clear();
    localClick.start();
    localClick.process(2048, false, nullptr, &localClockEvents);
    expect(localClockEvents.isEmpty(), "Metronome-only restart must suppress all outgoing master messages");
    // MIDI master phase must survive irregular callbacks without accumulating drift.
    for (double bpm : {120.0, 137.3, 300.0}) {
        ecm::MidiClockMaster master;
        juce::MidiBuffer generated;
        const double interval = 48000.0 * 60.0 / (bpm * 24.0);
        int pulse = 0;
        int starts = 0;
        int offset = 0;
        for (int callback = 0; callback < 800; ++callback) {
            const int frames = callback % 2 == 0 ? 97 : 1024;
            generated.clear();
            master.process(frames, 48000, bpm, 3, true, generated);
            for (const auto event : generated) {
                const auto message = event.getMessage();
                if (message.isMidiStart()) {
                    ++starts;
                    expect(offset == 0 && event.samplePosition == 0, "Master Start must precede first pulse");
                } else {
                    expect(message.isMidiClock(), "Running master must only emit clock pulses");
                    expect(offset + event.samplePosition == static_cast<int>(std::ceil(pulse * interval - 1e-7)),
                           "MIDI master must generate 24 PPQN without callback rounding drift");
                    ++pulse;
                }
            }
            offset += frames;
        }
        expect(starts == 1, "Master must send one Start per transport start");
        generated.clear();
        master.process(128, 48000, bpm, 4, true, generated);
        expect(generated.getNumEvents() == 1 && (*generated.begin()).getMessage().isMidiStop(),
               "Master must send Stop on transport stop");
        generated.clear();
        master.process(128, 48000, bpm, 4, true, generated);
        expect(generated.isEmpty(), "Stopped master must not send pulses or repeated Stops");
        master.process(128, 48000, bpm, 5, true, generated);
        expect(generated.getNumEvents() == 2 && (*generated.begin()).getMessage().isMidiStart(),
               "Restart must send Start followed by an immediate pulse");
        generated.clear();
        master.process(128, 48000, bpm, 5, false, generated);
        expect(generated.getNumEvents() == 1 && (*generated.begin()).getMessage().isMidiStop(),
               "Disabling master must stop receiving devices");
    }
    expect(ecm::distinctMidiClockDestinations({"port A", "port A", "port A"}) == std::array<bool, 3>{true, false, false},
           "Three zones sharing a port must receive only one clock stream");
    expect(ecm::distinctMidiClockDestinations({"port A", "port B", "port A"}) == std::array<bool, 3>{true, true, false},
           "Repeated nonadjacent zone destinations must be deduplicated");
    expect(ecm::distinctMidiClockDestinations({"", "port B", "port C"}) == std::array<bool, 3>{false, true, true},
           "Unconfigured zones must not receive clock");
    for (uint8_t group = 0; group < 3; ++group) {
        using Factory = juce::universal_midi_packets::Factory;
        expect(ecm::midiClockSystemPacket(0xf8, group) == Factory::makeTimingClock(group).data()[0],
               "Each MIDI 2 zone must receive a System Timing Clock on its own group");
        expect(ecm::midiClockSystemPacket(0xfa, group) == Factory::makeStart(group).data()[0],
               "MIDI 2 master Start encoding must match UMP");
        expect(ecm::midiClockSystemPacket(0xfc, group) == Factory::makeStop(group).data()[0],
               "MIDI 2 master Stop encoding must match UMP");
    }
    // Check that the sender honours deadlines and stops peers before shutdown.
    juce::CriticalSection sentLock;
    std::vector<std::pair<uint8_t, double>> sent;
    ecm::MidiClockOutput sender([&](uint8_t status) {
        const juce::ScopedLock lock(sentLock);
        sent.emplace_back(status, juce::Time::getMillisecondCounterHiRes());
    });
    sender.start();
    juce::MidiBuffer scheduled;
    scheduled.addEvent(juce::MidiMessage::midiStart(), 0);
    scheduled.addEvent(juce::MidiMessage::midiClock(), 2400);
    scheduled.addEvent(juce::MidiMessage::midiClock(), 4800);
    const auto startMs = juce::Time::getMillisecondCounterHiRes();
    sender.schedule(scheduled, startMs, 48000);
    for (int attempts = 0; attempts < 200; ++attempts) {
        { const juce::ScopedLock lock(sentLock); if (sent.size() == 3) break; }
        juce::Thread::sleep(5);
    }
    sender.stop();
    expect(sent.size() == 4, "Scheduled clock must send Start, two pulses, and shutdown Stop");
    if (sent.size() == 4) {
        expect(sent[0].first == 0xfa && sent[1].first == 0xf8 && sent[2].first == 0xf8 && sent[3].first == 0xfc,
               "Scheduled master transport order must be preserved");
        expect(sent[1].second >= startMs + 50 && sent[2].second >= startMs + 100,
               "Clock events must wait for their deadlines instead of bursting at callback start");
    }
    if (ok) std::cout << "EigenAudioBridge checks passed\n";
    return ok ? 0 : 1;
}
