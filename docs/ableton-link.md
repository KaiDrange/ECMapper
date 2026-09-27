# Ableton Link

## Build and verification

CMake fetches the pinned Link 4.1 SDK and bundled Asio. An existing recursive
checkout can be supplied with
`-DFETCHCONTENT_SOURCE_DIR_ABLETON_LINK=/absolute/path/to/link`.

Build and run `LinkClockTest` and `EigenAudioBridgeTest` for deterministic click,
meter, count-in, clock-correction, MIDI-clock, and audio-routing checks. Run
`LinkClockTest --network` on an otherwise empty Link network for two-peer
discovery, tempo exchange, bar phase, quantized launch, and optional transport
sync checks. It requires local multicast/network access.

In CLion, reload the CMake project after pulling these changes and select the
**LinkClockTest** CMake Application run configuration. The single-file
**LinkClockTest.cpp** configuration cannot build this test: it omits the include
paths, generated sample data, and JUCE/Link dependencies provided by CMake.

