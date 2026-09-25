#include "PanelComponent.h"
#include "AppStyle.h"

namespace ecm {

PanelComponent::PanelComponent(float widthFactorToUse, float heightFactorToUse)
    : widthFactor(widthFactorToUse), heightFactor(heightFactorToUse) {
}

void PanelComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Style::surface());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(Style::border());
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void PanelComponent::resized() {
}

} // namespace ecm
