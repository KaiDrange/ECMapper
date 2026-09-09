#pragma once

#include <JuceHeader.h>
#include <array>
#include "PerformanceEventSink.h"

namespace ecm {

enum class Vst3DirectEventKind {
    NoteOn,
    NoteOff,
    PolyPressure,
    NoteExpression,
    LegacyCC,
};

enum class Vst3NoteExpressionType {
    Tuning,
    Expression,
    Brightness,
};

struct Vst3DirectEvent {
    Vst3DirectEventKind kind = Vst3DirectEventKind::LegacyCC;
    Vst3NoteExpressionType expressionType = Vst3NoteExpressionType::Expression;
    int sampleOffset = 0;
    int busIndex = 0;
    int channel = 0;
    int noteNumber = -1;
    int noteId = -1;
    int controller = 0;
    double value = 0.0;
    double value2 = 0.0;
};

class Vst3DirectEventQueue {
public:
    void clear();
    void push(const Vst3DirectEvent& event);
    bool empty() const;
    std::vector<Vst3DirectEvent> drain();

private:
    juce::CriticalSection lock_;
    std::vector<Vst3DirectEvent> events_;
};

class Vst3DirectPerformanceEventSink final : public PerformanceEventSink {
public:
    explicit Vst3DirectPerformanceEventSink(Vst3DirectEventQueue& queue);

    void configureLayout(const juce::MPEZoneLayout& layout);
    void pushEvent(const PerformanceEvent& event) override;
    void reset();

private:
    int allocateNoteId(int channel, int noteNumber);
    int findNoteId(int channel, int noteNumber) const;
    void releaseNoteId(int channel, int noteNumber);
    static int noteIndex(int channel, int noteNumber);

    Vst3DirectEventQueue& queue_;
    std::array<int, 16 * 128> activeNoteIds_ {};
    int nextNoteId_ = 1;
};

}