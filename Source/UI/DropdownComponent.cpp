#include "DropdownComponent.h"

namespace ecm {

DropdownComponent::DropdownComponent() {
    addAndMakeVisible(label);
    addAndMakeVisible(box);
}

void DropdownComponent::resized() {
    auto area = getLocalBounds();
    if (labelAbove)
        label.setBounds(area.removeFromTop(area.getHeight() / 2));
    else
        label.setBounds(area.removeFromLeft(area.getWidth() / 2));
    box.setBounds(area);
}

void DropdownComponent::setLabelText(const juce::String& text, bool labelAboveBox) {
    label.setText(text, juce::dontSendNotification);
    this->labelAbove = labelAboveBox;
}

void DropdownComponent::addItem(const juce::String& text, int itemId) {
    box.addItem(text, itemId);
}

void DropdownComponent::setSelectedItemId(int id) {
    box.setSelectedId(id, juce::dontSendNotification);
}

} // namespace ecm
