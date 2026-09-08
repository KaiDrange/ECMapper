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

    static PerformanceEvent noteOn(int channel, int noteNumber, float velocity, int sampleOffset) {
        return { PerformanceEventKind::NoteOn, channel, noteNumber, -1, -1, 0, 0.0f, velocity, false, sampleOffset };
    }

    static PerformanceEvent noteOff(int channel, int noteNumber, float velocity, int sampleOffset) {
        return { PerformanceEventKind::NoteOff, channel, noteNumber, -1, -1, 0, 0.0f, velocity, false, sampleOffset };
    }

    static PerformanceEvent pitchBend(int channel, int noteNumber, float value, bool perNote, int sampleOffset) {
        return { PerformanceEventKind::PitchBend, channel, noteNumber, -1, -1, 0, value, 0.0f, perNote, sampleOffset };
    }

    static PerformanceEvent channelPressure(int channel, int noteNumber, float value, bool perNote, int sampleOffset) {
        return { PerformanceEventKind::ChannelPressure, channel, noteNumber, -1, -1, 0, value, 0.0f, perNote, sampleOffset };
    }

    static PerformanceEvent polyAftertouch(int channel, int noteNumber, float value, int sampleOffset) {
        return { PerformanceEventKind::PolyAftertouch, channel, noteNumber, -1, -1, 0, value, 0.0f, true, sampleOffset };
    }

    static PerformanceEvent controllerChange(int channel, int noteNumber, int controllerNumber, float value, bool perNote, int sampleOffset) {
        return { PerformanceEventKind::Controller, channel, noteNumber, -1, controllerNumber, 0, value, 0.0f, perNote, sampleOffset };
    }

    static PerformanceEvent programChange(int channel, int program, int sampleOffset) {
        return { PerformanceEventKind::ProgramChange, channel, -1, -1, -1, program, 0.0f, 0.0f, false, sampleOffset };
    }

    static PerformanceEvent allNotesOff(int channel, int sampleOffset) {
        return { PerformanceEventKind::AllNotesOff, channel, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset };
    }

    static PerformanceEvent midiStart(int sampleOffset) {
        return { PerformanceEventKind::MidiStart, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset };
    }

    static PerformanceEvent midiStop(int sampleOffset) {
        return { PerformanceEventKind::MidiStop, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset };
    }

    static PerformanceEvent midiContinue(int sampleOffset) {
        return { PerformanceEventKind::MidiContinue, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset };
    }
};

}