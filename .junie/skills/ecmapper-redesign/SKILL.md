---
name: ecmapper-redesign
description: Architectural guidelines and technical stack for the ECMapper unified redesign project.
---

# ECMapper Redesign Skill

Use this skill when working on the ECMapper project, specifically when adding new features, refactoring, or managing dependencies in the unified application/plugin structure.

## Core Architecture

- **Unified Project**: This project combines the legacy `EigenCore` and `ECMapper` into a single codebase.
- **Dual Targets**: The build system produces two main targets:
  - `ECMapper_App`: A standalone GUI application.
  - `ECMapper_Plugin`: A VST3 audio plugin.
- **Build Options**:
  - `ECMAPPER_BUILD_APP`: Toggle standalone application build (default: ON).
  - `ECMAPPER_BUILD_PLUGIN`: Toggle plugin build (default: ON).
- **CMake Presets**: Use `CMakePresets.json` to easily build specific targets for macOS and Windows.
  - Available Build Presets: `macos-debug-app`, `macos-debug-plugin`, `macos-release-app`, etc.
- **Source Organization**:
  - `Source/Core/`: Place all shared business logic, hardware communication (EigenLite), and MIDI processing here.
  - `Source/UI/`: Place all shared JUCE components and UI-related logic here.
  - `Source/`: Contains the entry points and boilerplate for the standalone app and plugin.

## Technical Stack

- **C++ Standard**: C++20 (mandatory).
- **Version**: v2.0-alpha (current).
- **Framework**: JUCE 9.0.1+ (native MIDI 2.0 and UMP support).
- **Hardware Abstraction**: EigenLite (located in `EigenLite/`).
- **Hardware Transition**: Initially use the current `EigenLite` version, but be prepared for a future transition to a more recent version.
- **MIDI**: Focus on MIDI 2.0 / Universal MIDI Packet (UMP) support.

## Guidelines

- **Shared Code**: Always prefer placing logic in `Source/Core` or `Source/UI` so it can be used by both the Standalone App and the Plugin.
- **Legacy Reference**: If you need to check how things were done previously, refer to the `old/` directory. Do NOT modify files in `old/`. To avoid IDE confusion, ensure that any archived code moved to `old/` has its internal `.git` metadata removed.
- **CMake**: Maintain the `CMakeLists.txt` at the root. Ensure any new source files are added to `SHARED_SOURCES` if they are shared, or to the specific target sources if they are target-specific.
- **Dependencies**: JUCE and EigenLite are subdirectories. Ensure they are correctly linked in CMake.
- **Code Style**: Mirror the organization of the OctaChainer2 project found in the `old/` folder.
