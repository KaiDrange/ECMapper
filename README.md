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

## Credits:

This project is made possible only because of the open-source work of TheTechnobear. ECMapper is completely dependent on his EigenLite API, 
and his work on "MEC", another alternative to EigenD has been an inspiration.

Thanks also to John Lambert/EigenLabs for making EigenD open-source so that community projects like these are even possible.

EigenLite: https://github.com/thetechnobear/EigenLite

JUCE Framework: https://juce.com/

JUCE is dual-licensed under the AGPLv3 and the commercial JUCE licence. See `JUCE/LICENSE.md` in this repository and the JUCE website for the full licence terms.