# ECMapper

ECMapper is an application and VST3 plugin for the EigenLabs Eigenharp instruments. It is intended as an
alternative to the official EigenD software. ECMapper interprets high resolution data from the hardware instruments
and converts it into MIDI. Both MIDI 1.0 and MIDI 2.0 are supported. Currently, only Mac is supported.

## Overview

Uses the JUCE Framework. EigenLite by the Technobear is used for hardware communication. More information can be found here:
https://ticticelectro.com/ECMapper

## String assignments

Select a note key in the layout editor and choose **String: 1–12** (or **String: None**).
For note keys, the first key sounds a note; later keys on that string bend it to their pitch
without retriggering and take over pressure, pitch bend and CC expression. Releasing a key
returns control to the most recently pressed accepted key still held. The last release ends
the original note. Targets beyond the configured pitch-bend range minus the key-expression
bend range are ignored until released. Strings are local to each device and zone.

Chords support up to six notes. Held chords combine into up to twelve strum pitches; chords have no separate
string selector and continue to sound normally when pressed. Older four-note layouts remain
compatible. Unassigned notes keep their normal playback behavior.

## Strumming keys

Choose **Type → Strum**, then set the **Output zone**, **Linked zone**, and **Linked string**.
A strum plays the current pitch from that string in the linked zone on the same device, using
the strum key's own strike and release velocity. Strummed pitches use the output zone's
transpose setting, ignoring the linked zone's transpose; device transpose still applies. The most recently pressed held note key or
chord selects the source. Held chords are combined into ascending pitches, with identical
notes in the same octave counted once; strings 1–12 play the lowest twelve distinct pitches.
A string without a pitch produces no note. For duplicate pitches, the most recently pressed
supplying chord controls linked expression. The linked source zone may be disabled: its held notes/chords
still supply pitches and expression while remaining silent. The strum output zone must be enabled.
Source keys sound normally when their own zone is enabled. A strum keeps its captured
pitch until released, even if the source changes or is released. **Expression** is on by default:
the strum key controls roll, yaw and pressure using its output zone's expression mappings. Turn
it off to use those values from the note/chord key that supplied the pitch instead. One source
can control several held strums. Releasing the source leaves their last expression values in
place; re-pressing it does not take over older strums. Strike and release velocity always come
from the strum key. Settings are saved with layouts.

## Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Credits:

This project is made possible only because of the open-source work of TheTechnobear. ECMapper is completely dependent on his EigenLite API, 
and his work on "MEC", another alternative to EigenD has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible.

EigenLite: https://github.com/thetechnobear/EigenLite

JUCE Framework: https://juce.com/

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. See `JUCE/LICENSE.md` in this repository and the JUCE website for the full licence terms.

Ableton Link is dual-licensed under GPLv2-or-later and a proprietary licence.
See [Ableton's licensing terms](https://github.com/Ableton/link/blob/Link-4.1/LICENSE.md).