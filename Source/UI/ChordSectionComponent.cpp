#include "ChordSectionComponent.h"

namespace ecm {

ChordSectionComponent::ChordSectionComponent() : chordNameInput("Name:", 0, 5, "", false) {
    addAndMakeVisible(chordNameInput);
    chordNameInput.addListener(this);
    for (int i = 0; i < 4; i++) {
        setNoteLabelText(i);
        chordNotes[i].setButton.setToggleable(true);
        chordNotes[i].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[i].setButton.setClickingTogglesState(true);
        addAndMakeVisible(chordNotes[i].label);
        addAndMakeVisible(chordNotes[i].setButton);
        addAndMakeVisible(chordNotes[i].clearButton);
    }

    chordNotes[0].setButton.onClick = [this] {
        chordNotes[1].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[2].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[3].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    };
    chordNotes[1].setButton.onClick = [this] {
        chordNotes[0].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[2].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[3].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    };
    chordNotes[2].setButton.onClick = [this] {
        chordNotes[0].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[1].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[3].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    };
    chordNotes[3].setButton.onClick = [this] {
        chordNotes[0].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[1].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[2].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    };

    chordNotes[0].clearButton.onClick = [this] { chordNotes[0].midiNoteNumber = -1; setNoteLabelText(0); sendChangeMessage(); };
    chordNotes[1].clearButton.onClick = [this] { chordNotes[1].midiNoteNumber = -1; setNoteLabelText(1); sendChangeMessage(); };
    chordNotes[2].clearButton.onClick = [this] { chordNotes[2].midiNoteNumber = -1; setNoteLabelText(2); sendChangeMessage(); };
    chordNotes[3].clearButton.onClick = [this] { chordNotes[3].midiNoteNumber = -1; setNoteLabelText(3); sendChangeMessage(); };
}

ChordSectionComponent::~ChordSectionComponent() = default;

void ChordSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = area.getHeight() * 0.04f;
    chordNameInput.setBounds(area.removeFromTop(lineHeight));
    for (auto & chordNote : chordNotes) {
        area.removeFromTop(lineHeight);
        chordNote.label.setBounds(area.removeFromTop(lineHeight));
        auto line = area.removeFromTop(lineHeight);
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
    return chordNameInput.getValue() + ";" +
        juce::String(chordNotes[0].midiNoteNumber) + ";" +
        juce::String(chordNotes[1].midiNoteNumber) + ";" +
        juce::String(chordNotes[2].midiNoteNumber) + ";" +
        juce::String(chordNotes[3].midiNoteNumber);
}

void ChordSectionComponent::updatePanelFromMessageString(const juce::String& msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    if (tokens.size() != 5) {
        chordNameInput.setValue("");
        for (int i = 0; i < 4; i++) {
            chordNotes[i].midiNoteNumber = -1;
            setNoteLabelText(i);
        }
        return;
    }
    
    chordNameInput.setValue(tokens[0]);
    for (int i = 0; i < 4; i++) {
        chordNotes[i].midiNoteNumber = tokens[i + 1].getIntValue();
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
    for (int i = 0; i < 4; i++) {
        if (chordNotes[i].setButton.getToggleState()) {
            chordNotes[i].midiNoteNumber = midiNoteNumber;
            setNoteLabelText(i);
            sendChangeMessage();
            break;
        }
    }
}

void ChordSectionComponent::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
    for (int i = 0; i < 4; i++) {
        chordNotes[i].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    }
}

void ChordSectionComponent::resetPanel() {
    chordNameInput.setValue("");
    for (int i = 0; i < 4; i++) {
        chordNotes[i].midiNoteNumber = -1;
        setNoteLabelText(i);
    }
}

} // namespace ecm
