#include "KeyConfigComponent.h"

KeyConfigComponent::KeyConfigComponent(LayoutWrapper::KeyId id, EigenharpKeyType keyType) : juce::DrawableButton("btn", juce::DrawableButton::ImageStretched) {
    this->keyType = keyType;
    this->keyId = id;
    setClickingTogglesState(true);
}

KeyConfigComponent::~KeyConfigComponent() {
}

void KeyConfigComponent::paint(juce::Graphics& g) {
    auto layoutKey = LayoutWrapper::getLayoutKey(keyId);
    auto area = getLocalBounds();
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (10.0f);
    juce::String keyText = layoutKey.mappingValue;
    if (layoutKey.keyMappingType == KeyMappingType::Note) {
        keyText = juce::MidiMessage::getMidiNoteName(keyText.getIntValue(), true, true, 3);
    }
        
    g.drawFittedText(keyText, getLocalBounds(),
                juce::Justification::centred, true);

    g.setColour(juce::Colour(Utility::keyColourEnumToColour(layoutKey.keyColour)));
    auto lightPosition = area.getX() + area.getWidth()/2.0f;
    g.drawEllipse(lightPosition-2, 3, 3, 3, 3);

    g.setColour(Utility::zoneEnumToColour(layoutKey.zone));
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

LayoutWrapper::KeyId KeyConfigComponent::getKeyId() const {
    return keyId;
}
