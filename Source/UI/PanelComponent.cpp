#include "PanelComponent.h"

namespace ecm {

PanelComponent::PanelComponent(float widthFactor, float heightFactor)
    : widthFactor(widthFactor), heightFactor(heightFactor) {
}

void PanelComponent::paint(juce::Graphics& g) {
    g.setColour(juce::Colours::grey.withAlpha(0.2f));
    g.drawRect(getLocalBounds(), 1);
}

void PanelComponent::resized() {
}

} // namespace ecm
