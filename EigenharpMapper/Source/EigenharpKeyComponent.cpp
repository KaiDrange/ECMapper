#include <JuceHeader.h>
#include "EigenharpKeyComponent.h"

//==============================================================================
EigenharpKeyComponent::EigenharpKeyComponent(EigenharpKeyType keyType)
{
    this->keyType = keyType;
}

EigenharpKeyComponent::~EigenharpKeyComponent()
{
}

void EigenharpKeyComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

    if (keyType == EigenharpKeyType::Normal) {
        g.setColour (juce::Colours::grey);
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
    }
    else if (keyType == EigenharpKeyType::Perc) {
        g.setColour (juce::Colours::grey);
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
    }
    else if (keyType == EigenharpKeyType::Button) {
        g.setColour (juce::Colours::grey);
        auto area = getLocalBounds();
        g.drawEllipse(area.getX(), area.getY(), area.getWidth(), area.getHeight(), 1);
    }

}

void EigenharpKeyComponent::resized()
{
}
