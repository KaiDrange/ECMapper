#include "TextInputComponent.h"

namespace ecm {

TextInputComponent::TextInputComponent(const juce::String& labelText,
                                       int minLength,
                                       int maxLength,
                                       const juce::String& legalChars,
                                       bool labelAboveInput)
    : minLength(minLength), maxLength(maxLength), legalChars(legalChars), labelAboveInput(labelAboveInput) {
    
    label.setText(labelText, juce::dontSendNotification);
    if (maxLength > 0)
        input.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(maxLength, legalChars), true);
    
    addAndMakeVisible(label);
    addAndMakeVisible(input);

    input.onFocusLost = [this] {
        sendChangeMessage();
    };
}

void TextInputComponent::resized() {
    auto area = getLocalBounds();
    if (labelAboveInput)
        label.setBounds(area.removeFromTop(area.getHeight() / 2));
    else
        label.setBounds(area.removeFromLeft(area.getWidth() / 2));
    input.setBounds(area);
}

juce::String TextInputComponent::getValue() const {
    return input.getText();
}

void TextInputComponent::setValue(const juce::String& text) {
    input.setText(text, juce::dontSendNotification);
}

void TextInputComponent::setLabelText(const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
}

void TextInputComponent::addListener(Listener* listenerToAdd) {
    listeners.add(listenerToAdd);
}

void TextInputComponent::removeListener(Listener* listenerToRemove) {
    listeners.remove(listenerToRemove);
}

void TextInputComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.textInputChanged(this); });
}

} // namespace ecm
