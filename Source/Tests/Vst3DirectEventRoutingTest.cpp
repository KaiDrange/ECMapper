#include <iostream>

#include <JuceHeader.h>

#include "Core/PerformanceEvent.h"
#include "Core/Vst3DirectEventQueue.h"

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool expectNear(float actual, float expected, float tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << message << " expected=" << expected << " actual=" << actual << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main()
{
    ecm::Vst3DirectEventQueue queue;
    ecm::Vst3DirectPerformanceEventSink sink(queue);

    sink.pushEvent(ecm::PerformanceEvent::channelPressure(3, -1, 0.0f, false, 0, 1));
    sink.pushEvent(ecm::PerformanceEvent::pitchBend(3, -1, 0.5f, false, 0, 1));
    sink.pushEvent(ecm::PerformanceEvent::controllerChange(3, -1, 74, 0.5f, false, 0, 1));
    sink.pushEvent(ecm::PerformanceEvent::noteOn(3, 64, 0.8f, 0, 1));
    sink.pushEvent(ecm::PerformanceEvent::pitchBend(3, 64, 0.625f, true, 0, 1, 48.0f));
    sink.pushEvent(ecm::PerformanceEvent::noteOff(3, 64, 0.0f, 0, 1));
    sink.pushEvent(ecm::PerformanceEvent::noteOn(4, 67, 0.8f, 0, 0));
    sink.pushEvent(ecm::PerformanceEvent::pitchBend(4, 67, 0.75f, true, 0, 0, 36.0f));
    sink.pushEvent(ecm::PerformanceEvent::noteOff(4, 67, 0.0f, 0, 0));

    const auto events = queue.drain();

    bool ok = true;
    ok &= expect(events.size() == 9, "the direct sink should queue the converted VST3 events for channel and per-note messages");

    if (events.size() == 9)
    {
        ok &= expect(events[0].kind == ecm::Vst3DirectEventKind::LegacyCC,
                     "channel pressure should be exported as a VST3 legacy MIDI controller event");
        ok &= expect(events[0].controller == Steinberg::Vst::ControllerNumbers::kAfterTouch,
                     "channel pressure should keep the VST3 aftertouch controller number instead of defaulting to CC 0");
        ok &= expect(events[0].busIndex == 1,
                     "channel pressure should stay on the originating zone bus");

        ok &= expect(events[1].kind == ecm::Vst3DirectEventKind::LegacyCC,
                     "channel pitch bend should be exported as a VST3 legacy MIDI controller event");
        ok &= expect(events[1].controller == Steinberg::Vst::ControllerNumbers::kPitchBend,
                     "channel pitch bend should keep the VST3 pitch-bend controller number instead of defaulting to CC 0");
        ok &= expect(std::abs(events[1].value2 - 0.5) < 1.0e-6,
                     "channel pitch bend should preserve its value in the VST3 secondary field");
        ok &= expect(events[1].busIndex == 1,
                     "channel pitch bend should stay on the originating zone bus");

        ok &= expect(events[2].kind == ecm::Vst3DirectEventKind::LegacyCC,
                     "plain controller messages should remain legacy MIDI controller events");
        ok &= expect(events[2].controller == 74,
                     "the configured CC number should be preserved for controller messages");
        ok &= expect(events[2].busIndex == 1,
                     "controller messages should stay on the originating zone bus");

        ok &= expect(events[4].kind == ecm::Vst3DirectEventKind::NoteExpression,
                     "per-note pitch bend should be exported as a VST3 note-expression event");
        ok &= expect(events[4].expressionType == ecm::Vst3NoteExpressionType::Tuning,
                     "per-note pitch bend should use the VST3 tuning note-expression type");
        ok &= expectNear(static_cast<float>(events[4].value), 0.55f, 1.0e-6f,
                         "MPE per-note pitch bend should be rescaled from the zone pitch-bend range into the VST3 tuning range");
        ok &= expect(events[4].busIndex == 1,
                     "MPE per-note pitch bend should stay on the originating zone bus");

        ok &= expect(events[7].kind == ecm::Vst3DirectEventKind::NoteExpression,
                     "single-channel per-note pitch bend should also be exported as a VST3 note-expression event");
        ok &= expectNear(static_cast<float>(events[7].value), 0.575f, 1.0e-6f,
                         "single-channel per-note pitch bend should be rescaled from the configured channel range into the VST3 tuning range");
        ok &= expect(events[7].busIndex == 0,
                     "single-channel per-note pitch bend should stay on the originating zone bus");
    }

    return ok ? 0 : 1;
}