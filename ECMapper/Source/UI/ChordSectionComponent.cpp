#include "ChordSectionComponent.h"

ChordSectionComponent::ChordSectionComponent() : chordNameInput("Name:", 0, 5, "", false) {
    addAndMakeVisible(chordNameInput);
    chordNameInput.addListener(this);
    for (int i = 0; i < 4; i++) {
        setNoteLabelText(i);
        chordNotes[i].setButton.setButtonText("Set");
        chordNotes[i].setButton.setToggleable(true);
        chordNotes[i].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
        chordNotes[i].setButton.setClickingTogglesState(true);
        chordNotes[i].clearButton.setButtonText("Clear");
        addAndMakeVisible(chordNotes[i].label);
        addAndMakeVisible(chordNotes[i].setButton);
        addAndMakeVisible(chordNotes[i].clearButton);
    }
    
    chordNotes[0].clearButton.onClick = [&] { chordNotes[0].midiNoteNumber = -1; setNoteLabelText(0); sendChangeMessage(); };
    chordNotes[1].clearButton.onClick = [&] { chordNotes[1].midiNoteNumber = -1; setNoteLabelText(1); sendChangeMessage(); };
    chordNotes[2].clearButton.onClick = [&] { chordNotes[2].midiNoteNumber = -1; setNoteLabelText(2); sendChangeMessage(); };
    chordNotes[3].clearButton.onClick = [&] { chordNotes[3].midiNoteNumber = -1; setNoteLabelText(3); sendChangeMessage(); };
}

ChordSectionComponent::~ChordSectionComponent() {
}

void ChordSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = area.getHeight()*0.04;
    chordNameInput.setBounds(area.removeFromTop(lineHeight));
    for (int i = 0; i < 4; i++) {
        area.removeFromTop(lineHeight);
        chordNotes[i].label.setBounds(area.removeFromTop(lineHeight));
        auto line = area.removeFromTop(lineHeight);
        chordNotes[i].setButton.setBounds(line.removeFromLeft(line.getWidth()*0.5));
        chordNotes[i].clearButton.setBounds(line);
    }
}

void ChordSectionComponent::setNoteLabelText(int noteIndex) {
    chordNotes[noteIndex].label.setText("Note "
            + juce::String(noteIndex+1)
            + ": "
            + (chordNotes[noteIndex].midiNoteNumber > -1
                        ? juce::MidiMessage::getMidiNoteName(chordNotes[noteIndex].midiNoteNumber, true, true, 3)
                        : "None")
        , juce::NotificationType::dontSendNotification);
}

juce::String ChordSectionComponent::getMessageString() {
    // chordName;note1;note2;note3;note4
    return chordNameInput.getValue() + ";" +
        juce::String(chordNotes[0].midiNoteNumber) + ";" +
        juce::String(chordNotes[1].midiNoteNumber) + ";" +
        juce::String(chordNotes[2].midiNoteNumber) + ";" +
        juce::String(chordNotes[3].midiNoteNumber);
}

void ChordSectionComponent::updatePanelFromMessageString(juce::String msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    if (tokens.size() != 5)
        return;
    
    chordNameInput.setValue(tokens[0]);
    for (int i = 0; i < 4; i++) {
        chordNotes[i].midiNoteNumber = tokens[i+1].getIntValue();
        setNoteLabelText(i);
    }
}

void ChordSectionComponent::addListener(Listener* listenerToAdd) {
    listeners.add(listenerToAdd);
}

void ChordSectionComponent::removeListener(Listener* listenerToRemove) {
    jassert(listeners.contains(listenerToRemove));
    listeners.remove(listenerToRemove);
}

void ChordSectionComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.valuesChanged(this); });
}

void ChordSectionComponent::textInputChanged(TextInputComponent*) {
    sendChangeMessage();
}

void ChordSectionComponent::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    for (int i = 0; i < 4; i++) {
        if (chordNotes[i].setButton.getToggleState()) {
            chordNotes[i].midiNoteNumber = midiNoteNumber;
            setNoteLabelText(i);
            sendChangeMessage();
            break;
        }
    }
}

void ChordSectionComponent::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    for (int i = 0; i < 4; i++) {
        chordNotes[i].setButton.setToggleState(false, juce::NotificationType::dontSendNotification);
    }
}
