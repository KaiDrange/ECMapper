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
    if (labelAbove)
        label.setBounds(area.removeFromTop(area.getHeight()*0.5));
    else
        label.setBounds(area.removeFromLeft(area.getWidth()*0.5));
    box.setBounds(area);
}

void DropdownComponent::setLabelText(const juce::String text, const bool labelAboveBox) {
    label.setText(text, juce::dontSendNotification);
    this->labelAbove = labelAboveBox;
}

void DropdownComponent::addItem(const juce::String text, const int itemId) {
    box.addItem(text, itemId);
}

void DropdownComponent::setSelectedItemId(int id) {
    box.setSelectedId(id);
}
