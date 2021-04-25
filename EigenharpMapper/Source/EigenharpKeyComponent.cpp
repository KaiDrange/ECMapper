#include <JuceHeader.h>
#include "EigenharpKeyComponent.h"

EigenharpKeyComponent::EigenharpKeyComponent(const EigenharpKeyType keyType, const MappedKey *mappedKey)
    : juce::DrawableButton("btn", juce::DrawableButton::ImageStretched) {
    this->keyType = keyType;
    this->mappedKey = mappedKey;
    setClickingTogglesState(true);
}

EigenharpKeyComponent::~EigenharpKeyComponent() {
}

void EigenharpKeyComponent::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (10.0f);
    juce::String keyText = mappedKey->mapping;
    if (mappedKey->mappingType == KeyMappingType::Note) {
        keyText = juce::MidiMessage::getMidiNoteName(keyText.getIntValue(), true, true, 3);
    }
        
    g.drawFittedText(keyText, getLocalBounds(),
                juce::Justification::centred, true);

    juce::Colour lightColour;
    switch(mappedKey->colour) {
        case KeyColour::Off:
            lightColour = juce::Colours::transparentBlack;
            break;
        case KeyColour::Green:
            lightColour = juce::Colours::green;
            break;
        case KeyColour::Yellow:
            lightColour = juce::Colours::yellow;
            break;
        case KeyColour::Red:
            lightColour = juce::Colours::red;
            break;
        default:
            lightColour = juce::Colours::transparentBlack;
            break;
    }
    
    g.setColour(lightColour);
    auto lightPosition = area.getX() + area.getWidth()/2.0f;
    g.drawEllipse(lightPosition-2, 3, 3, 3, 3);

    g.setColour(zoneColours[mappedKey->zone]);
    if (keyType == EigenharpKeyType::Normal) {
        g.drawRoundedRectangle(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 5, 2);
    }
    else if (keyType == EigenharpKeyType::Perc) {
        g.drawRoundedRectangle(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 10, 2);
    }
    else if (keyType == EigenharpKeyType::Button) {
        g.drawEllipse(area.getX(), area.getY(), area.getWidth(), area.getHeight(), 1);
    }
}
