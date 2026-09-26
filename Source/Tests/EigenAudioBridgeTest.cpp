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
    bridge.setHostActive(false);
    bridge.prepare(44100);
    bridge.setHostActive(true);
    expect(bridge.start().isNotEmpty(), "Unsupported sample rate must be rejected");
    if (ok) std::cout << "EigenAudioBridge checks passed\n";
    return ok ? 0 : 1;
}
