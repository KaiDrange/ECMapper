#include "Vst3DirectEventQueue.h"

namespace ecm {

namespace {

Vst3NoteExpressionType mapControllerExpressionType(const int controller)
{
    switch (controller) {
        case 74: return Vst3NoteExpressionType::Brightness;
        case 11:
        case 7:  return Vst3NoteExpressionType::Expression;
        default: return Vst3NoteExpressionType::Expression;
    }
}

double convertPerNotePitchBendToVst3Tuning(const PerformanceEvent& event)
{
    constexpr double vst3TuningRangeSemitones = 240.0;
    const auto centeredValue = static_cast<double>(event.value) * 2.0 - 1.0;
    const auto sourceRange = std::max(0.0, static_cast<double>(event.pitchbendRange));
    const auto scaled = centeredValue * sourceRange / vst3TuningRangeSemitones;
    return juce::jlimit(0.0, 1.0, 0.5 + scaled);
}

}

void Vst3DirectEventQueue::clear()
{
    const juce::ScopedLock sl(lock_);
    events_.clear();
}

void Vst3DirectEventQueue::push(const Vst3DirectEvent& event)
{
    const juce::ScopedLock sl(lock_);
    events_.push_back(event);
}

bool Vst3DirectEventQueue::empty() const
{
    const juce::ScopedLock sl(lock_);
    return events_.empty();
}

std::vector<Vst3DirectEvent> Vst3DirectEventQueue::drain()
{
    const juce::ScopedLock sl(lock_);
    auto drained = std::move(events_);
    events_.clear();
    return drained;
}

Vst3DirectPerformanceEventSink::Vst3DirectPerformanceEventSink(Vst3DirectEventQueue& queue)
    : queue_(queue)
{
    reset();
}

void Vst3DirectPerformanceEventSink::pushEvent(const PerformanceEvent& event)
{
    Vst3DirectEvent directEvent;
    directEvent.busIndex = juce::jlimit(0, 2, event.zoneIndex < 0 ? 0 : event.zoneIndex);
    directEvent.sampleOffset = event.sampleOffset;
    directEvent.channel = juce::jlimit(0, 15, event.channel - 1);
    directEvent.noteNumber = event.noteNumber;
    directEvent.value = event.kind == PerformanceEventKind::NoteOff ? event.velocity : event.value;

    switch (event.kind) {
        case PerformanceEventKind::NoteOn:
            directEvent.kind = Vst3DirectEventKind::NoteOn;
            directEvent.value = event.velocity;
            directEvent.noteId = allocateNoteId(event.channel, event.noteNumber);
            queue_.push(directEvent);
            return;

        case PerformanceEventKind::NoteOff:
            directEvent.kind = Vst3DirectEventKind::NoteOff;
            directEvent.value = event.velocity;
            directEvent.noteId = findNoteId(event.channel, event.noteNumber);
            queue_.push(directEvent);
            releaseNoteId(event.channel, event.noteNumber);
            return;

        case PerformanceEventKind::ChannelPressure:
            if (event.perNote && event.noteNumber >= 0) {
                directEvent.kind = Vst3DirectEventKind::PolyPressure;
                directEvent.noteId = findNoteId(event.channel, event.noteNumber);
                queue_.push(directEvent);
                return;
            }
            directEvent.controller = Steinberg::Vst::ControllerNumbers::kAfterTouch;
            break;

        case PerformanceEventKind::PitchBend:
            if (event.perNote && event.noteNumber >= 0) {
                directEvent.kind = Vst3DirectEventKind::NoteExpression;
                directEvent.expressionType = Vst3NoteExpressionType::Tuning;
                directEvent.noteId = findNoteId(event.channel, event.noteNumber);
                directEvent.pitchbendRange = event.pitchbendRange;
                directEvent.value = convertPerNotePitchBendToVst3Tuning(event);
                queue_.push(directEvent);
                return;
            }
            directEvent.controller = Steinberg::Vst::ControllerNumbers::kPitchBend;
            directEvent.value2 = directEvent.value;
            break;

        case PerformanceEventKind::Controller:
            if (event.perNote && event.noteNumber >= 0 && (event.controller == 74 || event.controller == 11 || event.controller == 7)) {
                directEvent.kind = Vst3DirectEventKind::NoteExpression;
                directEvent.expressionType = mapControllerExpressionType(event.controller);
                directEvent.noteId = findNoteId(event.channel, event.noteNumber);
                queue_.push(directEvent);
                return;
            }
            directEvent.controller = event.controller;
            break;

        case PerformanceEventKind::ProgramChange:
            directEvent.controller = 130;
            directEvent.value = juce::jlimit(0.0, 127.0, static_cast<double>(event.program));
            break;

        case PerformanceEventKind::MidiStart:
            directEvent.controller = 137;
            break;

        case PerformanceEventKind::MidiContinue:
            directEvent.controller = 138;
            break;

        case PerformanceEventKind::MidiStop:
            directEvent.controller = 139;
            break;

        case PerformanceEventKind::PolyAftertouch:
            directEvent.kind = Vst3DirectEventKind::PolyPressure;
            directEvent.noteId = findNoteId(event.channel, event.noteNumber);
            queue_.push(directEvent);
            return;

        case PerformanceEventKind::AllNotesOff:
            reset();
            directEvent.controller = 123;
            break;
    }

    directEvent.kind = Vst3DirectEventKind::LegacyCC;
    queue_.push(directEvent);
}

void Vst3DirectPerformanceEventSink::reset()
{
    activeNoteIds_.fill(-1);
    nextNoteId_ = 1;
}

int Vst3DirectPerformanceEventSink::allocateNoteId(int channel, int noteNumber)
{
    const int index = noteIndex(channel, noteNumber);
    if (index < 0)
        return -1;

    const int noteId = nextNoteId_++;
    activeNoteIds_[static_cast<size_t>(index)] = noteId;
    return noteId;
}

int Vst3DirectPerformanceEventSink::findNoteId(int channel, int noteNumber) const
{
    const int index = noteIndex(channel, noteNumber);
    return index >= 0 ? activeNoteIds_[static_cast<size_t>(index)] : -1;
}

void Vst3DirectPerformanceEventSink::releaseNoteId(int channel, int noteNumber)
{
    const int index = noteIndex(channel, noteNumber);
    if (index >= 0)
        activeNoteIds_[static_cast<size_t>(index)] = -1;
}

int Vst3DirectPerformanceEventSink::noteIndex(int channel, int noteNumber)
{
    if (channel < 1 || channel > 16 || noteNumber < 0 || noteNumber > 127)
        return -1;
    return (channel - 1) * 128 + noteNumber;
}

}