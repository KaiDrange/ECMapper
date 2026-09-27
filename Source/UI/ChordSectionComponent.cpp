#include "ChordSectionComponent.h"

namespace ecm {

ChordSectionComponent::ChordSectionComponent() : chordNameInput("Name:", 0, 5, "", false) {
    addAndMakeVisible(chordNameInput);
    chordNameInput.addListener(this);
    for (int i = 0; i < 6; i++) {
        setNoteLabelText(i);
        chordNotes[i].setButton.setToggleable(true);
        chordNotes[i].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[i].setButton.setClickingTogglesState(true);
        addAndMakeVisible(chordNotes[i].label);
        addAndMakeVisible(chordNotes[i].setButton);
        addAndMakeVisible(chordNotes[i].clearButton);
    }

    for (int i = 0; i < 6; ++i) {
        chordNotes[i].setButton.onClick = [this, i] {
            for (int j = 0; j < 6; ++j)
                if (j != i) chordNotes[j].setButton.setToggleState(false, juce::dontSendNotification);
        };
        chordNotes[i].clearButton.onClick = [this, i] {
            chordNotes[i].midiNoteNumber = -1;
            setNoteLabelText(i);
            sendChangeMessage();
        };
    }
}

ChordSectionComponent::~ChordSectionComponent() = default;

void ChordSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = static_cast<float>(area.getHeight()) * 0.04f;
    chordNameInput.setBounds(area.removeFromTop(static_cast<int>(lineHeight)));
    for (auto & chordNote : chordNotes) {
        area.removeFromTop(static_cast<int>(lineHeight));
        chordNote.label.setBounds(area.removeFromTop(static_cast<int>(lineHeight)));
        auto line = area.removeFromTop(static_cast<int>(lineHeight));
        chordNote.setButton.setBounds(line.removeFromLeft(line.getWidth() / 2));
        chordNote.clearButton.setBounds(line);
    }
}

void ChordSectionComponent::setNoteLabelText(int noteIndex) {
    chordNotes[noteIndex].label.setText("Note "
            + juce::String(noteIndex + 1)
            + ": "
            + (chordNotes[noteIndex].midiNoteNumber > -1
                        ? juce::MidiMessage::getMidiNoteName(chordNotes[noteIndex].midiNoteNumber, true, true, 3)
                        : "None")
        , juce::NotificationType::dontSendNotification);
}

juce::String ChordSectionComponent::getMessageString() const
{
    juce::String result = chordNameInput.getValue();
    for (const auto& note : chordNotes)
        result += ";" + juce::String(note.midiNoteNumber);
    return result;
}

void ChordSectionComponent::updatePanelFromMessageString(const juce::String& msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    if (tokens.size() != 5 && tokens.size() != 7) {
        chordNameInput.setValue("");
        for (int i = 0; i < 6; i++) {
            chordNotes[i].midiNoteNumber = -1;
            setNoteLabelText(i);
        }
        return;
    }
    
    chordNameInput.setValue(tokens[0]);
    for (int i = 0; i < 6; i++) {
        chordNotes[i].midiNoteNumber = i + 1 < tokens.size() ? tokens[i + 1].getIntValue() : -1;
        setNoteLabelText(i);
    }
}

void ChordSectionComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.valuesChanged(this); });
}

void ChordSectionComponent::textInputChanged(TextInputComponent*) {
    sendChangeMessage();
}

void ChordSectionComponent::visibilityChanged() {
    if (isVisible()) sendChangeMessage();
}

void ChordSectionComponent::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float) {
    for (int i = 0; i < 6; i++) {
        if (chordNotes[i].setButton.getToggleState()) {
            chordNotes[i].midiNoteNumber = midiNoteNumber;
            setNoteLabelText(i);
            sendChangeMessage();
            break;
        }
    }
}

void ChordSectionComponent::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
    for (int i = 0; i < 6; i++) {
        chordNotes[i].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    }
}

void ChordSectionComponent::resetPanel() {
    chordNameInput.setValue("");
    for (int i = 0; i < 6; i++) {
        chordNotes[i].midiNoteNumber = -1;
        setNoteLabelText(i);
    }
}

} // namespace ecm
