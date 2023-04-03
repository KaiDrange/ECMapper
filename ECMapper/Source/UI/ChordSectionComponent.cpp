#include <JuceHeader.h>
#include "ChordSectionComponent.h"

ChordSectionComponent::ChordSectionComponent() : chordNameInput("Name:", 0, 5, "", false)
{
    addAndMakeVisible(chordNameInput);
    chordNameInput.addListener(this);
    for (int i = 0; i < 4; i++)
    {
        chordNotes[i].label.setText("Note " + juce::String(i+1) + ":", juce::NotificationType::dontSendNotification);
        chordNotes[i].setButton.setButtonText("Set");
        chordNotes[i].clearButton.setButtonText("Clear");
        addAndMakeVisible(chordNotes[i].label);
        addAndMakeVisible(chordNotes[i].setButton);
        addAndMakeVisible(chordNotes[i].clearButton);
    }
}

ChordSectionComponent::~ChordSectionComponent() {
}

void ChordSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = area.getHeight()*0.04;
    chordNameInput.setBounds(area.removeFromTop(lineHeight));
    for (int i = 0; i < 4; i++)
    {
        area.removeFromTop(lineHeight);
        chordNotes[i].label.setBounds(area.removeFromTop(lineHeight));
        auto line = area.removeFromTop(lineHeight);
        chordNotes[i].setButton.setBounds(line.removeFromLeft(line.getWidth()*0.5));
        chordNotes[i].clearButton.setBounds(line);
    }
}

juce::String ChordSectionComponent::getMessageString() {
    juce::String note1;
    juce::String note2;
    juce::String note3;
    juce::String note4;

    note1 = "10";
    note2 = "12";
    note3 = "14";
    note4 = "16";
    // chordName;note1;note2;note3;note4
    return chordNameInput.getValue() + ";" +
        note1 + ";" +
        note2 + ";" +
        note3 + ";" +
        note4;
}

void ChordSectionComponent::updatePanelFromMessageString(juce::String msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    if (tokens.size() != 5)
        return;
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
