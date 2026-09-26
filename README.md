# ECMapper

ECMapper is an application and VST3 plugin for the EigenLabs Eigenharp instruments. It is intended as an
alternative to the official EigenD software. ECMapper interprets high resolution data from the hardware instruments
and converts it into MIDI. Both MIDI 1.0 and MIDI 2.0 are supported. Currently, only Mac is supported.

## Overview

Uses the JUCE Framework. EigenLite by the Technobear is used for hardware communication. More information can be found here:
https://ticticelectro.com/ECMapper

## Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Temporary Eigenharp audio test

In Host mode, Connections > Start plays `assets/B1_WhileTheShadowsGrow.wav`
through each enabled Alpha/Tau headphone output. Stop stops playback; Start
always restarts the file. Playback ends at EOF. The metronome volume knob controls
this test source, and headphone gain controls the instrument hardware. The knob
shows dB: the existing default is -57 dB. Each device has a Host-only
**Limit to -30 dB** toggle, enabled by default (including older saved settings).
Turning it off allows gain up to 0 dB without raising the current volume.
Turning it on clamps both the knob and saved gain to -30 dB. The limit setting
is saved per device. Older UI values were native register settings, not percentages:
for example, 33 means -94 dB. For a listening test, try -40 dB, then increase
as needed toward -30 dB.

Use a **48 kHz** JUCE audio device or plugin host session. The standalone currently
has no sample-rate selector; on macOS, select 48 kHz for its audio device in Audio
MIDI Setup before launching. Enable headphones on the connected device before
pressing Start. No sound is routed to the computer's normal audio output.

The WAV is preloaded during audio preparation to avoid disk access in the audio
callback. It is read from the source assets directory, not embedded in the app;
`ECMAPPER_TEST_AUDIO_FILE` is a CMake cache path override. The bridge accumulates
512 frames, queues them to the hardware thread and uses EigenLite period 1.
Stopped playback sends silence. Already submitted USB audio may finish after Stop.
Other sample rates, clock/Link synchronization and remote playback control are not
implemented by this temporary feature.

Hardware test confirmed by the user on 2026-09-25: Tau playback was audible and
played back perfectly when headphone gain was turned to maximum. The UI showed
**-33 dB** at that maximum. Preserve this observed value: it differs from the
intended -30 dB UI ceiling and should be checked next session. This confirms the
user's listening test, not a measured latency/drift or extended stability test.

Build and run `EigenAudioBridgeTest` to check accumulation, stereo order,
stop/restart, EOF, volume and queue overflow without hardware. Pass an absolute
WAV path as its first argument to also check decoding that file. These checks do
not establish stable playback or clock drift on a physical instrument.

## Credits:

This project is made possible only because of the open-source work of TheTechnobear. ECMapper is completely dependent on his EigenLite API, 
and his work on "MEC", another alternative to EigenD has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible.

EigenLite: https://github.com/thetechnobear/EigenLite

JUCE Framework: https://juce.com/

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. See `JUCE/LICENSE.md` in this repository and the JUCE website for the full licence terms.