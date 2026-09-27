# ECMapper

ECMapper is an application and VST3 plugin for the EigenLabs Eigenharp instruments. It is intended as an
alternative to the official EigenD software. ECMapper interprets high resolution data from the hardware instruments
and converts it into MIDI. Both MIDI 1.0 and MIDI 2.0 are supported. Currently, only Mac is supported.

## Overview

Uses the JUCE Framework. EigenLite by the Technobear is used for hardware communication. More information can be found here:
https://ticticelectro.com/ECMapper

## String assignments

Select a note key in the layout editor and choose **String: 1–6** (or **String: None**).
For note keys, the first key sounds a note; later keys on that string bend it to their pitch
without retriggering and take over pressure, pitch bend and CC expression. Releasing a key
returns control to the most recently pressed accepted key still held. The last release ends
the original note. Targets beyond the configured pitch-bend range minus the key-expression
bend range are ignored until released. Strings are local to each device and zone.

Chords support up to six notes. Their slots correspond to strings 1–6; chords have no separate
string selector and continue to sound normally when pressed. Older four-note layouts remain
compatible. Unassigned notes keep their normal playback behavior.

## Strumming keys

Choose **Type → Strum**, then set the **Output zone**, **Linked zone**, and **Linked string**.
A strum plays the current pitch from that string in the linked zone on the same device, using
the strum key's own strike and release velocity. The most recently pressed held note key or
chord supplies the pitch; chord note slots correspond to strings 1–6. An empty chord slot or
no held source produces no note. Source keys still sound normally. A strum keeps its captured
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