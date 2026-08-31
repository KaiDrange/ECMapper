#include "NumberInputComponent.h"

namespace ecm {

NumberInputComponent::NumberInputComponent(const juce::String& labelText,
                                           int maxDigits,
                                           int minValue,
                                           int maxValue,
                                           bool labelAboveInput)
    : minValue(minValue), maxValue(maxValue), labelAboveInput(labelAboveInput) {
    
    label.setText(labelText, juce::dontSendNotification);
    input.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(maxDigits, "-0123456789"), true);
    input.setJustification(juce::Justification::right);
    addAndMakeVisible(label);
    addAndMakeVisible(input);

    input.onTextChange = [this] {
        auto newVal = getValue();
        if (newVal < this->minValue) {
            input.setText(juce::String(this->minValue), juce::dontSendNotification);
        } else if (newVal > this->maxValue) {
            input.setText(juce::String(this->maxValue), juce::dontSendNotification);
        }
    };
    
    input.onFocusLost = [this] {
        sendChangeMessage();
    };
}

void NumberInputComponent::resized() {
    auto area = getLocalBounds();
    if (labelAboveInput)
        label.setBounds(area.removeFromTop(juce::jmax(14, (int) std::round(area.getHeight() * 0.42f))));
    else
        label.setBounds(area.removeFromLeft(area.getWidth() / 2));
    input.setBounds(area);
}

int NumberInputComponent::getValue() const {
    return input.getText().getIntValue();
}

void NumberInputComponent::setValue(int number) {
    number = std::clamp(number, minValue, maxValue);
    input.setText(juce::String(number), juce::dontSendNotification);
}

void NumberInputComponent::setLabelText(const juce::String& text) {
    label.setText(text, juce::dontSendNotification);
}

void NumberInputComponent::addListener(Listener* listenerToAdd) {
    listeners.add(listenerToAdd);
}

void NumberInputComponent::removeListener(Listener* listenerToRemove) {
    listeners.remove(listenerToRemove);
}

void NumberInputComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.numberInputChanged(this); });
}

} // namespace ecm
