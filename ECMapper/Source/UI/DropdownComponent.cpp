#include <JuceHeader.h>
#include "DropdownComponent.h"

DropdownComponent::DropdownComponent() {
    addAndMakeVisible(label);
    addAndMakeVisible(box);
}

DropdownComponent::~DropdownComponent() {
}

void DropdownComponent::resized() {
    auto area = getLocalBounds();
    label.setBounds(area.removeFromLeft(area.getWidth()*0.5));
    box.setBounds(area);
}

void DropdownComponent::setLabelText(const juce::String text) {
    label.setText(text, juce::dontSendNotification);
}

void DropdownComponent::addItem(const juce::String text, const int itemId) {
    box.addItem(text, itemId);
}

void DropdownComponent::setSelectedItemId(int id) {
    box.setSelectedId(id);
}
