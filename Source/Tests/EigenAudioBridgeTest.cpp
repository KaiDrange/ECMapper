#include "Core/EigenAudioBridge.h"
#include <cmath>
#include <iostream>

namespace {
bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
float sampleAt(int frame) { return static_cast<float>(frame - 10000) / 32768.0f; }

bool writeFixture(const juce::File& file) {
    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) return false;
    constexpr int frames = 1025;
    stream.write("RIFF", 4);
    stream.writeInt(36 + frames * 4);
    stream.write("WAVEfmt ", 8);
    stream.writeInt(16);
    stream.writeShort(1);
    stream.writeShort(2);
    stream.writeInt(48000);
    stream.writeInt(48000 * 4);
    stream.writeShort(4);
    stream.writeShort(16);
    stream.write("data", 4);
    stream.writeInt(frames * 4);
    for (int i = 0; i < frames; ++i) {
        stream.writeShort(static_cast<short>(i - 10000));
        stream.writeShort(static_cast<short>(10000 - i));
    }
    stream.flush();
    return stream.getStatus().wasOk();
}
bool isSilence(const ecm::EigenAudioBridge::Block& block) {
    return std::all_of(block.stereo.begin(), block.stereo.end(), [](float value) { return value == 0.0f; });
}
}

int main(int argc, char* argv[]) {
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("ecmapper-audio-bridge", ".wav");
    if (!writeFixture(file)) return 1;
    ecm::EigenAudioBridge bridge;
    ecm::EigenAudioBridge::Block block;
    bool ok = true;
    bridge.prepare(file, 48000);
    ok &= expect(bridge.start().isNotEmpty(), "Client/inactive bridge must reject Start");
    bridge.setHostActive(true);
    ok &= expect(bridge.start().isEmpty(), "48 kHz fixture should start");
    for (int i = 0; i < 3; ++i) bridge.process(128);
    ok &= expect(!bridge.pop(block), "Must accumulate a complete 512-frame quantum");
    bridge.process(128);
    ok &= expect(bridge.pop(block), "Four 128-frame callbacks must produce one block");
    for (int i = 0; i < 512; ++i) {
        ok &= expect(std::abs(block.stereo[static_cast<std::size_t>(2 * i)] - sampleAt(i)) < 0.000001f,
                     "Left channel/order mismatch");
        ok &= expect(std::abs(block.stereo[static_cast<std::size_t>(2 * i + 1)] + sampleAt(i)) < 0.000001f,
                     "Right channel/interleaving mismatch");
    }
    bridge.process(63);
    bridge.process(17);
    bridge.process(432);
    ok &= expect(bridge.pop(block) && std::abs(block.stereo[0] - sampleAt(512)) < 0.000001f,
                 "Awkward host blocks must preserve sample continuity");
    bridge.process(512);
    ok &= expect(bridge.pop(block) && std::abs(block.stereo[0] - sampleAt(1024)) < 0.000001f,
                 "Final partial file block must preserve the last sample");
    ok &= expect(std::all_of(block.stereo.begin() + 2, block.stereo.end(), [](float x) { return x == 0.0f; }),
                 "EOF must pad with silence");
    ok &= expect(!bridge.isPlaying(), "EOF must clear playback status");
    bridge.process(512);
    ok &= expect(bridge.pop(block) && isSilence(block), "Continue sending silence after EOF");

    bridge.start();
    bridge.process(512);
    bridge.stop();
    ok &= expect(!bridge.pop(block), "Stop must discard queued audio from the previous play command");
    bridge.process(512);
    ok &= expect(bridge.pop(block) && isSilence(block), "Stopped stream must send silence");
    bridge.start();
    bridge.process(256);
    bridge.stop();
    bridge.process(256);
    bridge.start();
    bridge.process(512);
    ok &= expect(bridge.pop(block) && std::abs(block.stereo[0] - sampleAt(0)) < 0.000001f,
                 "Restart must discard partial accumulation and rewind");

    bridge.setHostActive(false);
    bridge.setVolume(0.5f);
    bridge.prepare(file, 48000);
    bridge.setHostActive(true);
    bridge.start();
    bridge.process(512);
    ok &= expect(bridge.pop(block) && std::abs(block.stereo[0] - sampleAt(0) * 0.5f) < 0.000001f,
                 "Metronome volume must scale PCM samples");
    bridge.process(512 * (ecm::EigenAudioBridge::queueBlocks + 2));
    ok &= expect(bridge.droppedBlocks() == 3, "Bounded FIFO must count dropped blocks without blocking");
    bridge.setHostActive(false);
    ok &= expect(!bridge.pop(block) && !bridge.isPlaying(), "Leaving Host mode must discard pending audio");

    bridge.prepare(file, 44100);
    bridge.setHostActive(true);
    ok &= expect(bridge.start().isNotEmpty(), "Non-48 kHz host must be rejected");
    bridge.process(512);
    ok &= expect(!bridge.pop(block), "Unsupported host rate must not enqueue audio");
    file.deleteFile();
    bridge.prepare(file, 48000);
    ok &= expect(bridge.start().isNotEmpty(), "Missing WAV must be reported without crashing");
    if (argc > 1) {
        bridge.setHostActive(false);
        bridge.prepare(juce::File(juce::String::fromUTF8(argv[1])), 48000);
        bridge.setHostActive(true);
        ok &= expect(bridge.start().isEmpty(), "Supplied test WAV must decode and start");
        bridge.process(512);
        ok &= expect(bridge.pop(block), "Supplied test WAV must produce a transport block");
    }
    if (ok) std::cout << "EigenAudioBridge checks passed\n";
    return ok ? 0 : 1;
}
