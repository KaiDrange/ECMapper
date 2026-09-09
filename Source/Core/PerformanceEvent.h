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
    int outputPort = 0;

    PerformanceEvent withOutputPort(int newOutputPort) const {
        auto copy = *this;
        copy.outputPort = newOutputPort;
        return copy;
    }

    static PerformanceEvent noteOn(int channel, int noteNumber, float velocity, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::NoteOn, channel, noteNumber, -1, -1, 0, 0.0f, velocity, false, sampleOffset, outputPort };
    }

    static PerformanceEvent noteOff(int channel, int noteNumber, float velocity, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::NoteOff, channel, noteNumber, -1, -1, 0, 0.0f, velocity, false, sampleOffset, outputPort };
    }

    static PerformanceEvent pitchBend(int channel, int noteNumber, float value, bool perNote, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::PitchBend, channel, noteNumber, -1, -1, 0, value, 0.0f, perNote, sampleOffset, outputPort };
    }

    static PerformanceEvent channelPressure(int channel, int noteNumber, float value, bool perNote, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::ChannelPressure, channel, noteNumber, -1, -1, 0, value, 0.0f, perNote, sampleOffset, outputPort };
    }

    static PerformanceEvent polyAftertouch(int channel, int noteNumber, float value, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::PolyAftertouch, channel, noteNumber, -1, -1, 0, value, 0.0f, true, sampleOffset, outputPort };
    }

    static PerformanceEvent controllerChange(int channel, int noteNumber, int controllerNumber, float value, bool perNote, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::Controller, channel, noteNumber, -1, controllerNumber, 0, value, 0.0f, perNote, sampleOffset, outputPort };
    }

    static PerformanceEvent programChange(int channel, int program, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::ProgramChange, channel, -1, -1, -1, program, 0.0f, 0.0f, false, sampleOffset, outputPort };
    }

    static PerformanceEvent allNotesOff(int channel, int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::AllNotesOff, channel, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, outputPort };
    }

    static PerformanceEvent midiStart(int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::MidiStart, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, outputPort };
    }

    static PerformanceEvent midiStop(int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::MidiStop, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, outputPort };
    }

    static PerformanceEvent midiContinue(int sampleOffset, int outputPort = 0) {
        return { PerformanceEventKind::MidiContinue, 1, -1, -1, -1, 0, 0.0f, 0.0f, false, sampleOffset, outputPort };
    }
};

}