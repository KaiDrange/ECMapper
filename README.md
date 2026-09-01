# ECMapper (Combined) v2.0-alpha

This is a fresh redesign of the EigenCore and ECMapper applications, combined into a single application/plugin.

## Overview

Initially supporting Mac (Standalone and VST3), with Windows support planned.
Uses JUCE with experimental MIDI 2.0 support.
Uses EigenLite for hardware communication.
Windows builds are client-only for now: OSC/network features work, but direct device communication is disabled.

## Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

On Windows, use the `windows-debug` or `windows-release` preset. Those presets turn off the EigenLite hardware path and build the client mode only.
That setup also requires a Visual Studio or clang-cl toolchain; MinGW is not supported by JUCE in this project.

## Legacy Code
The old separate versions of EigenCore and ECMapper are archived in the `old/` directory.
