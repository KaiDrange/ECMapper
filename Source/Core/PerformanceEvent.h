#pragma once

namespace ecm {

enum class OutputTransportMode {
    LegacyMidi = 0,
    UmpMidi = 1,
    Vst3Direct = 2,
};

enum class PerformanceEventKind {
    NoteOn,
    NoteOff,
    PitchBend,
    ChannelPressure,
    PolyAftertouch,
    Controller,
    ProgramChange,
    AllNotesOff,
    MidiStart,
    MidiStop,
    MidiContinue,
};

struct PerformanceEvent {
    PerformanceEventKind kind {};
    int channel = 1;
    int noteNumber = -1;
    int noteId = -1;
    int controller = -1;
    int program = 0;
    float value = 0.0f;
    float velocity = 0.0f;
    bool perNote = false;
    int sampleOffset = 0;
    int zoneIndex = -1;

    static PerformanceEvent noteOn(int channel, int noteNumber, float velocity, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::NoteOn, channel, noteNumber, -1, -1, 0, 0.0f, velocity, false, sampleOffset, zoneIndex };
    }

    static PerformanceEvent noteOff(int channel, int noteNumber, float velocity, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::NoteOff, channel, noteNumber, -1, -1, 0, 0.0f, velocity, false, sampleOffset, zoneIndex };
    }

    static PerformanceEvent pitchBend(int channel, int noteNumber, float value, bool perNote, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::PitchBend, channel, noteNumber, -1, -1, 0, value, 0.0f, perNote, sampleOffset, zoneIndex };
    }

    static PerformanceEvent channelPressure(int channel, int noteNumber, float value, bool perNote, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::ChannelPressure, channel, noteNumber, -1, -1, 0, value, 0.0f, perNote, sampleOffset, zoneIndex };
    }

    static PerformanceEvent polyAftertouch(int channel, int noteNumber, float value, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::PolyAftertouch, channel, noteNumber, -1, -1, 0, value, 0.0f, true, sampleOffset, zoneIndex };
    }

    static PerformanceEvent controllerChange(int channel, int noteNumber, int controllerNumber, float value, bool perNote, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::Controller, channel, noteNumber, -1, controllerNumber, 0, value, 0.0f, perNote, sampleOffset, zoneIndex };
    }

    static PerformanceEvent programChange(int channel, int program, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::ProgramChange, channel, -1, -1, -1, program, 0.0f, 0.0f, false, sampleOffset, zoneIndex };
    }

    static PerformanceEvent allNotesOff(int channel, int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::AllNotesOff, channel, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, zoneIndex };
    }

    static PerformanceEvent midiStart(int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::MidiStart, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, zoneIndex };
    }

    static PerformanceEvent midiStop(int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::MidiStop, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, zoneIndex };
    }

    static PerformanceEvent midiContinue(int sampleOffset, int zoneIndex = -1) {
        return { PerformanceEventKind::MidiContinue, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, zoneIndex };
    }
};

}