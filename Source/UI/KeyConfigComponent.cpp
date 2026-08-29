#include "KeyConfigComponent.h"

namespace ecm {

KeyConfigComponent::KeyConfigComponent(LayoutWrapper::KeyId id, EigenharpKeyType keyType, juce::AudioProcessorValueTreeState& pluginState) 
    : juce::DrawableButton("btn", juce::DrawableButton::ImageStretched), 
      keyType(keyType), 
      keyId(id), 
      pluginState(pluginState) {
    setClickingTogglesState(true);
}

void KeyConfigComponent::paint(juce::Graphics& g) {
    auto layoutKey = LayoutWrapper::getLayoutKey(keyId, pluginState.state);
    auto area = getLocalBounds();
    
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    juce::String keyText = layoutKey.mappingValue;
    
    if (layoutKey.keyMappingType == KeyMappingType::Note) {
        keyText = juce::MidiMessage::getMidiNoteName(keyText.getIntValue(), true, true, 3);
    } else if (layoutKey.keyMappingType == KeyMappingType::Chord) {
        juce::StringArray chordParts;
        Utils::splitString(keyText, ";", chordParts);
        if (chordParts.size() > 0 && chordParts[0].isNotEmpty()) {
            keyText = chordParts[0];
        } else {
            keyText = "Chrd";
        }
    } else if (layoutKey.keyMappingType == KeyMappingType::MidiMsg) {
        juce::StringArray midiMsgParts;
        Utils::splitString(keyText, ";", midiMsgParts);
        if (midiMsgParts.size() > 1) {
            if (midiMsgParts[1] == "Realtime") keyText = "RT";
            else if (midiMsgParts[1] == "AllNotesOff") keyText = "!";
            else keyText = midiMsgParts[1];
        }
    } else {
        keyText = "";
    }
        
    g.drawFittedText(keyText, getLocalBounds(), juce::Justification::centred, true);

    g.setColour(Utils::keyColourEnumToColour(layoutKey.keyColour));
    auto lightPosition = area.getX() + area.getWidth() / 2.0f;
    g.fillEllipse(lightPosition - 2.5f, 3.0f, 5.0f, 5.0f);

    g.setColour(Utils::zoneEnumToColour(layoutKey.zone));
    if (keyType == EigenharpKeyType::Normal) {
        g.drawRoundedRectangle(area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() - 2.0f, 5.0f, 2.0f);
    } else if (keyType == EigenharpKeyType::Perc) {
        g.drawRoundedRectangle(area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() - 2.0f, 10.0f, 2.0f);
    } else if (keyType == EigenharpKeyType::Button) {
        g.drawEllipse(area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() - 2.0f, 2.0f);
    }
}

} // namespace ecm
