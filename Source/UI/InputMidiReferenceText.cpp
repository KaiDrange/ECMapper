#include "InputMidiReferenceText.h"

namespace ecm {

juce::String getInputMidiReferenceText()
{
    return juce::String(juce::CharPointer_UTF8(
        "Standalone ECMapper responds to these external MIDI input commands:\n\n"
        "• Program Change 0-15: load preset slot 1-16.\n"
        "• CC 22 on channels 1-3: set transpose for zone 1-3.\n"
        "• CC 22 on channel 4: set transpose for all three zones at once.\n"
        "  Value 64 = 0 semitones; each value step = 1 semitone, so the range is -64 to +63.\n"
        "• CC 23 on channels 1-3: disable or enable zone 1-3.\n"
        "• CC 23 on channel 4: disable or enable all three zones at once.\n"
        "  Values 0-63 = off, 64-127 = on."));
}

} // namespace ecm