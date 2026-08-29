# ECMapper (Combined) v2.0-alpha

This is a fresh redesign of the EigenCore and ECMapper applications, combined into a single application/plugin.

## Overview

Initially supporting Mac (Standalone and VST3), with Windows support planned.
Uses JUCE with experimental MIDI 2.0 support.
Uses EigenLite for hardware communication.

## Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Legacy Code
The old separate versions of EigenCore and ECMapper are archived in the `old/` directory.
