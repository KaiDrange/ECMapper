#include "InputMidiReferenceText.h"

namespace ecm {

juce::String getInputMidiReferenceText()
{
    return juce::String(juce::CharPointer_UTF8(
        "Standalone ECMapper responds to these external MIDI input commands:\n\n"
        "• Program Change 0-15: load preset slot 1-16.\n"
        "• MIDI channel 1 applies incoming CC commands globally to Alpha, Tau, and Pico.\n"
        "• MIDI channel 2 targets Alpha, channel 3 targets Tau, and channel 4 targets Pico.\n"
        "• CC 22, CC 23, and CC 24 set transpose for zone 1, zone 2, and zone 3 respectively.\n"
        "  Value 64 = 0 semitones; each value step = 1 semitone, so the range is -64 to +63.\n"
        "• CC 25, CC 26, and CC 27 disable or enable zone 1, zone 2, and zone 3 respectively.\n"
        "  Values 0-63 = off, 64-127 = on.\n"
        "• While mapping notes to layouts, note messages on MIDI channels 1-4 are all accepted."));
}

} // namespace ecm