#include <JuceHeader.h>
#include "ZonePanelComponent.h"

ZonePanelComponent::ZonePanelComponent(int zoneNumber, float widthFactor, float heightFactor): PanelComponent(widthFactor, heightFactor) {
    this->zoneNumber = zoneNumber;
}

ZonePanelComponent::~ZonePanelComponent() {
}

void ZonePanelComponent::paint(juce::Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("ZonePanelComponent" + juce::String(zoneNumber), getLocalBounds(),
                juce::Justification::centred, true);
}

void ZonePanelComponent::resized() {
}
