#include <JuceHeader.h>
#include "EigenharpKeyComponent.h"

//==============================================================================
EigenharpKeyComponent::EigenharpKeyComponent(EigenharpKeyType keyType)
    : juce::DrawableButton("btn", juce::DrawableButton::ImageStretched)
{
    this->keyType = keyType;
    setClickingTogglesState(true);
}

EigenharpKeyComponent::~EigenharpKeyComponent()
{
}

void EigenharpKeyComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (10.0f);
    g.drawFittedText ("C#2", getLocalBounds(),
                juce::Justification::centred, true);

    g.setColour (juce::Colours::green);
    
    auto lightPosition = area.getX() + area.getWidth()/2.0f;
    g.drawEllipse(lightPosition-2, 3, 3, 3, 3);

    if (keyType == EigenharpKeyType::Normal) {
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 5, 2);
    }
    else if (keyType == EigenharpKeyType::Perc) {
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 10, 2);
    }
    else if (keyType == EigenharpKeyType::Button) {
        g.setColour (juce::Colours::black);
        g.drawEllipse(area.getX(), area.getY(), area.getWidth(), area.getHeight(), 1);
    }

}
