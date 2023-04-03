#include <JuceHeader.h>
#include "TextInputComponent.h"

TextInputComponent::TextInputComponent(const juce::String labelText,
                                           const int minLength,
                                           const int maxLength,
                                           const juce::String legalChars,
                                           const bool labelAboveInput) {
    this->labelAboveInput = labelAboveInput;
    label.setText(labelText, juce::dontSendNotification);

    if (legalChars.isEmpty())
        this->legalChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-#";
    
    input.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(maxLength, this->legalChars), true);
    addAndMakeVisible(label);
    addAndMakeVisible(input);

    this->maxLength = maxLength;
    this->minLength = minLength;
    
    input.onTextChange = [=] {
        auto newText = getValue();
        if (newText.length() >= minLength && newText.length() <= maxLength) {
            input.setText(newText);
        }
    };
    
    input.onFocusLost = [&] {
        sendChangeMessage();
    };
}

TextInputComponent::~TextInputComponent() {
}

void TextInputComponent::resized() {
    auto area = getLocalBounds();
    if (labelAboveInput)
        label.setBounds(area.removeFromTop(area.getHeight()*0.5));
    else
        label.setBounds(area.removeFromLeft(area.getWidth()*0.5));
    input.setBounds(area);
}

juce::String TextInputComponent::getValue() {
    return input.getText();
}

void TextInputComponent::setValue(juce::String text) {
    input.setText(text);
}

void TextInputComponent::setLabelText(juce::String text) {
    label.setText(text, juce::dontSendNotification);
}

void TextInputComponent::addListener(Listener* listenerToAdd) {
    listeners.add(listenerToAdd);
}

void TextInputComponent::removeListener(Listener* listenerToRemove) {
    jassert(listeners.contains(listenerToRemove));
    listeners.remove(listenerToRemove);
}

void TextInputComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.textInputChanged(this); });
}
