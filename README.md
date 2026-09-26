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

## Eigenharp headphone metronome

In Host mode, select **MIDI clock master** in Connections, set BPM and time
signature, then press Start. The metronome plays through every enabled Alpha/Tau
headphone output until Stop. Start always begins a new bar. `assets/click1.wav`
is the first-beat accent; `assets/click2.wav` plays the remaining beats. Both are
mono 48 kHz samples, embedded in the app and duplicated to left and right.
BPM counts quarter notes; /8 signatures click on each eighth note. “None” plays
unaccented quarter notes. Tempo and meter can be changed while playing.
The metronome volume knob controls click level; headphone gain controls the
instrument hardware. The headphone gain knob
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

The clicks are decoded during audio preparation, with no allocation or file I/O
in the audio callback. Beat timing uses the audio sample clock, retaining fractional
beat lengths across callbacks. The bridge accumulates
128 frames and queues each block immediately; the hardware thread sends it on its
next poll, without waiting for a 512-frame group. Each device keeps the period
sequence 1, 0, 0, 0 across writes (including silence and Start/Stop). Newly enabled
outputs begin with period 1. FIFO capacity remains 3584 frames, with no prefill.
Use a 128-frame JUCE buffer to avoid larger host callbacks batching these writes.

The user confirmed four consecutive 128-frame writes with periods 1/0/0/0 worked
from a fresh Tau state, including with a 128-frame JUCE buffer. Those tests still
accumulated 512 frames before dispatch. This build tests immediate 128-frame
dispatch; playback stability and actual latency have not yet been confirmed.
The earlier period-1-on-every-write experiment may have left the Tau in a bad
state: restoring 512-frame writes only recovered sound after a full power-off
and reconnect. Start new timing experiments from a fully power-cycled Tau.
Stopped playback sends silence. Already submitted USB audio may finish after Stop.
When a plugin host reports offline rendering (`isNonRealtime()`), Eigenharp audio
production stops and queued/partial metronome audio is discarded. Returning to real-time
processing starts a fresh transport timing sequence and sends silence; press Start
to restart the metronome. Normal plugin MIDI processing continues. Audio already
submitted to USB may finish playing. This depends on the host reporting offline mode;
real-time exports are not automatically muted.
Other sample rates, clock/Link synchronization and remote playback control are not
implemented. External clock modes must be switched to local MIDI clock master
mode before starting the metronome.

Hardware test confirmed by the user on 2026-09-25: Tau playback was audible and
played back perfectly when headphone gain was turned to maximum. The UI showed
**-33 dB** at that maximum. Preserve this observed value: it differs from the
intended -30 dB UI ceiling and should be checked next session. This confirms the
user's listening test, not a measured latency/drift or extended stability test.

Build and run `EigenAudioBridgeTest` to check the bundled sample formats,
beat/accent timing (including fractional beat lengths), mono duplication,
stop/restart, offline rendering, volume and queue overflow without hardware.
These checks do not establish stability or clock drift on a physical instrument.

## Credits:

This project is made possible only because of the open-source work of TheTechnobear. ECMapper is completely dependent on his EigenLite API, 
and his work on "MEC", another alternative to EigenD has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible.

EigenLite: https://github.com/thetechnobear/EigenLite

JUCE Framework: https://juce.com/

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. See `JUCE/LICENSE.md` in this repository and the JUCE website for the full licence terms.