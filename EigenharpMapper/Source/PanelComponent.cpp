#include <JuceHeader.h>
#include "PanelComponent.h"

//==============================================================================
PanelComponent::PanelComponent(float widthFactor, float heightFactor)
{
    this->widthFactor = widthFactor;
    this->heightFactor = heightFactor;
}

PanelComponent::~PanelComponent()
{
}

void PanelComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
}

void PanelComponent::resized()
{
}
