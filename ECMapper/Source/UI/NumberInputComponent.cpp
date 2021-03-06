#include <JuceHeader.h>
#include "NumberInputComponent.h"

NumberInputComponent::NumberInputComponent(const juce::String labelText,
                                           const int maxDigits,
                                           const int minValue,
                                           const int maxValue,
                                           const bool labelAboveInput) {
    this->labelAboveInput = labelAboveInput;
    label.setText(labelText, juce::dontSendNotification);
    input.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(maxDigits, "-0123456789"), true);
    input.setJustification(juce::Justification::right);
    addAndMakeVisible(label);
    addAndMakeVisible(input);

    this->minValue = minValue;
    this->maxValue = maxValue;
    
    input.onTextChange = [=] {
        auto newVal = getValue();
        if (newVal < minValue) {
            input.setText(juce::String(minValue));
        }
        else if (newVal > maxValue) {
            input.setText(juce::String(maxValue));
        }
    };
    
    input.onFocusLost = [&] {
        sendChangeMessage();
    };
}

NumberInputComponent::~NumberInputComponent() {
}

void NumberInputComponent::resized() {
    auto area = getLocalBounds();
    if (labelAboveInput)
        label.setBounds(area.removeFromTop(area.getHeight()*0.5));
    else
        label.setBounds(area.removeFromLeft(area.getWidth()*0.5));
    input.setBounds(area);
}

int NumberInputComponent::getValue() {
    return input.getText().getIntValue();
}

void NumberInputComponent::setValue(int number) {
    if (number > maxValue)
        number = maxValue;
    if (number < minValue)
        number = minValue;
    
    input.setText(juce::String(number));
}

void NumberInputComponent::setLabelText(juce::String text) {
    label.setText(text, juce::dontSendNotification);
}

void NumberInputComponent::addListener(Listener* listenerToAdd) {
    listeners.add(listenerToAdd);
}

void NumberInputComponent::removeListener(Listener* listenerToRemove) {
    jassert(listeners.contains(listenerToRemove));
    listeners.remove(listenerToRemove);
}

void NumberInputComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.numberInputChanged(this); });
}
