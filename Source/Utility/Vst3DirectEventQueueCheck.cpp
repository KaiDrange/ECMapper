#include <JuceHeader.h>
#include <iostream>
#include "Core/ExpressionEmissionPolicy.h"
#include "Core/PerformanceEvent.h"
#include "Core/Vst3DirectEventQueue.h"

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
        std::cerr << message << std::endl;
    return condition;
}

}

int main()
{
    bool ok = true;

    ok &= expect(!ecm::usesIndependentPerNoteExpression(ecm::OutputTransportMode::LegacyMidi, false),
                 "Legacy MIDI should not treat note expression as independently addressable.");
    ok &= expect(!ecm::usesIndependentPerNoteExpression(ecm::OutputTransportMode::UmpMidi, false),
                 "UMP MIDI should still wait for remote per-note support before using independent note expression.");
    ok &= expect(!ecm::usesIndependentPerNoteExpression(ecm::OutputTransportMode::Vst3Direct, false),
                 "Direct VST should now use MPE-style channel events instead of independent note-expression events.");
    ok &= expect(ecm::createExpressionEmissionPolicy(ecm::OutputTransportMode::Vst3Direct)->shouldEmitContinuousUpdate(1),
                 "Direct VST should allow continuous updates on every incoming message.");

    ok &= expect(ecm::resolveRuntimeOutputMode(false, false, ecm::OutputTransportMode::Vst3Direct) == ecm::OutputTransportMode::Vst3Direct,
                 "Plugin Direct VST mode should resolve to the direct transport even when MIDI 2.0 mode is off.");

    ecm::Vst3DirectEventQueue queue;
    ecm::Vst3DirectPerformanceEventSink sink { queue };

    juce::MPEZoneLayout layout;
    layout.setLowerZone(4, 48, 2);
    layout.setUpperZone(3, 48, 2);
    sink.configureLayout(layout);

    sink.pushEvent(ecm::PerformanceEvent::noteOn(3, 60, 0.8f, 0).withOutputPort(0));
    sink.pushEvent(ecm::PerformanceEvent::pitchBend(3, 60, 0.6f, true, 1).withOutputPort(0));
    sink.pushEvent(ecm::PerformanceEvent::channelPressure(3, 60, 0.4f, true, 2).withOutputPort(0));
    sink.pushEvent(ecm::PerformanceEvent::pitchBend(3, -1, 0.25f, false, 3).withOutputPort(0));
    sink.pushEvent(ecm::PerformanceEvent::channelPressure(3, -1, 0.1f, false, 4).withOutputPort(0));
    sink.pushEvent(ecm::PerformanceEvent::noteOff(3, 60, 0.2f, 5).withOutputPort(0));

    sink.pushEvent(ecm::PerformanceEvent::noteOn(14, 67, 0.7f, 6).withOutputPort(1));
    sink.pushEvent(ecm::PerformanceEvent::controllerChange(14, -1, 74, 0.5f, false, 7).withOutputPort(1));
    sink.pushEvent(ecm::PerformanceEvent::noteOff(14, 67, 0.1f, 8).withOutputPort(1));
    sink.pushEvent(ecm::PerformanceEvent::programChange(5, 11, 9).withOutputPort(2));

    const auto events = queue.drain();

    ok &= expect(events.size() == 10, "Expected ten direct VST3 events.");
    ok &= expect(events[0].kind == ecm::Vst3DirectEventKind::NoteOn && events[0].channel == 2 && events[0].busIndex == 0,
                 "Lower-zone note-on should stay on its actual VST3 channel.");
    ok &= expect(events[1].kind == ecm::Vst3DirectEventKind::LegacyCC && events[1].channel == 2 && events[1].controller == 129 && events[1].busIndex == 0,
                 "Per-note pitch bend should become a VST3 legacy pitch-bend event on the note channel.");
    ok &= expect(juce::roundToInt(events[1].value * 127.0) == 102 && juce::roundToInt(events[1].value2 * 127.0) == 76,
                 "Per-note pitch bend should preserve the MIDI 14-bit value as legacy CC bytes.");
    ok &= expect(events[2].kind == ecm::Vst3DirectEventKind::LegacyCC && events[2].channel == 2 && events[2].controller == 128 && events[2].busIndex == 0,
                 "Per-note channel pressure should remain VST3 legacy aftertouch on the note channel after reverting the poly-pressure experiment.");
    ok &= expect(events[3].kind == ecm::Vst3DirectEventKind::LegacyCC && events[3].channel == 2 && events[3].controller == 129 && events[3].busIndex == 0,
                 "Channel pitch bend should become a VST3 legacy pitch-bend event on the original channel.");
    ok &= expect(juce::roundToInt(events[3].value * 127.0) == 0 && juce::roundToInt(events[3].value2 * 127.0) == 32,
                 "Channel pitch bend should preserve the MIDI 14-bit value as legacy CC bytes.");
    ok &= expect(events[4].kind == ecm::Vst3DirectEventKind::LegacyCC && events[4].channel == 2 && events[4].controller == 128 && events[4].busIndex == 0,
                 "Channel pressure must no longer fall through as CC0 bank select in Direct VST.");
    ok &= expect(events[5].kind == ecm::Vst3DirectEventKind::NoteOff && events[5].channel == 2 && events[5].noteId == events[0].noteId && events[5].busIndex == 0,
                 "Lower-zone note-off should stay on the original VST3 channel with the same noteId.");
    ok &= expect(events[6].kind == ecm::Vst3DirectEventKind::NoteOn && events[6].channel == 13 && events[6].busIndex == 1,
                 "Upper-zone note-on should stay on its actual VST3 channel.");
    ok &= expect(events[7].kind == ecm::Vst3DirectEventKind::LegacyCC && events[7].channel == 13 && events[7].controller == 74 && events[7].busIndex == 1,
                 "Direct VST CC74 should stay a legacy MIDI CC event on the note channel after reverting the note-expression experiment.");
    ok &= expect(events[8].kind == ecm::Vst3DirectEventKind::NoteOff && events[8].channel == 13 && events[8].noteId == events[6].noteId && events[8].busIndex == 1,
                 "Upper-zone note-off should stay on the original VST3 channel with the same noteId.");
    ok &= expect(events[9].kind == ecm::Vst3DirectEventKind::LegacyCC && events[9].channel == 4 && events[9].controller == 130 && events[9].busIndex == 2,
                 "Third-port program changes should preserve their requested VST3 output bus.");

    if (!ok)
        return 1;

    std::cout << "Vst3DirectEventQueueCheck passed" << std::endl;
    return 0;
}