#include "KeyConfigComponent.h"

KeyConfigComponent::KeyConfigComponent(KeyConfig::KeyId id, EigenharpKeyType keyType, juce::ValueTree parentTree)
    : juce::DrawableButton("btn", juce::DrawableButton::ImageStretched), keyConfig(id, keyType, parentTree) {
    this->keyType = keyType;
    setClickingTogglesState(true);
}

KeyConfigComponent::~KeyConfigComponent() {
}

void KeyConfigComponent::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (10.0f);
    juce::String keyText = keyConfig.getMappingValue();
    if (keyConfig.getMappingType() == KeyMappingType::Note) {
        keyText = juce::MidiMessage::getMidiNoteName(keyText.getIntValue(), true, true, 3);
    }
        
    g.drawFittedText(keyText, getLocalBounds(),
                juce::Justification::centred, true);

    g.setColour(juce::Colour(Utility::keyColourEnumToColour(keyConfig.getKeyColour())));
    auto lightPosition = area.getX() + area.getWidth()/2.0f;
    g.drawEllipse(lightPosition-2, 3, 3, 3, 3);

    g.setColour(Utility::zoneEnumToColour(keyConfig.getZone()));
    if (keyType == EigenharpKeyType::Normal) {
        g.drawRoundedRectangle(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 5, 2);
    }
    else if (keyType == EigenharpKeyType::Perc) {
        g.drawRoundedRectangle(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 10, 2);
    }
    else if (keyType == EigenharpKeyType::Button) {
        g.drawEllipse(area.getX()+1, area.getY()+1, area.getWidth()-2, area.getHeight()-2, 2);
    }
}

KeyConfig::KeyId KeyConfigComponent::getKeyId() const {
    return keyConfig.getKeyId();
}
