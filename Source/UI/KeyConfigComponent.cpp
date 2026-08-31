#include "KeyConfigComponent.h"
#include "AppStyle.h"

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

    auto keyBounds = area.toFloat().reduced(1.0f);
    auto keyRadius = keyType == EigenharpKeyType::Button ? 8.0f : (keyType == EigenharpKeyType::Perc ? 6.0f : 5.0f);
    auto isSelected = getToggleState();

    auto baseFill = juce::Colour(0xff11161c);
    auto faceFill = juce::Colour(0xff1b222b);
    auto highlightFill = juce::Colour(0xff2a3340);
    auto shadowFill = juce::Colour(0xff0a0d11);
    auto rimFill = juce::Colour(0xffcfd7df).withAlpha(0.12f);

    if (keyType == EigenharpKeyType::Perc)
    {
        baseFill = juce::Colour(0xff121820);
        faceFill = juce::Colour(0xff1f2731);
        highlightFill = juce::Colour(0xff313c4a);
    }
    else if (keyType == EigenharpKeyType::Button)
    {
        baseFill = juce::Colour(0xff10151b);
        faceFill = juce::Colour(0xff1a2029);
        highlightFill = juce::Colour(0xff28313d);
    }

    if (isSelected)
    {
        baseFill = baseFill.interpolatedWith(Style::accent(), 0.22f);
        faceFill = faceFill.interpolatedWith(Style::accentStrong(), 0.12f);
    }

    juce::ColourGradient outerGrad(juce::Colour(baseFill).brighter(0.10f),
                                   keyBounds.getCentreX(), keyBounds.getY(),
                                   shadowFill,
                                   keyBounds.getCentreX(), keyBounds.getBottom(),
                                   false);
    g.setGradientFill(outerGrad);
    g.fillRoundedRectangle(keyBounds, keyRadius);

    auto rim = keyBounds.reduced(1.5f);
    g.setColour(rimFill);
    g.drawRoundedRectangle(rim, keyRadius - 0.75f, 1.0f);

    auto face = keyBounds.reduced(3.0f);
    juce::ColourGradient faceGrad(highlightFill,
                                  face.getCentreX() - face.getWidth() * 0.35f,
                                  face.getY(),
                                  faceFill,
                                  face.getCentreX() + face.getWidth() * 0.3f,
                                  face.getBottom(),
                                  false);
    g.setGradientFill(faceGrad);
    g.fillRoundedRectangle(face, keyRadius - 2.0f);

    auto inset = face.reduced(face.getWidth() * 0.18f, face.getHeight() * 0.16f);
    juce::ColourGradient dome(juce::Colour(0xffffffff).withAlpha(0.12f),
                              inset.getCentreX() - inset.getWidth() * 0.15f,
                              inset.getY() + inset.getHeight() * 0.05f,
                              juce::Colour(0xff000000).withAlpha(0.40f),
                              inset.getCentreX(),
                              inset.getBottom(),
                              false);
    g.setGradientFill(dome);
    g.fillEllipse(inset);

    auto gloss = face.removeFromTop(juce::jmax(2.0f, face.getHeight() * 0.22f));
    g.setColour(juce::Colour(0xffffffff).withAlpha(isSelected ? 0.18f : 0.10f));
    g.fillRoundedRectangle(gloss, keyRadius - 2.0f);

    g.setColour(Style::background().withAlpha(0.35f));
    g.drawLine(keyBounds.getX() + 3.0f, keyBounds.getY() + 3.0f,
               keyBounds.getRight() - 3.0f, keyBounds.getBottom() - 3.0f, 0.6f);

    if (isSelected)
    {
        g.setColour(Style::accentStrong().withAlpha(0.50f));
        g.drawRoundedRectangle(keyBounds.reduced(0.5f), keyRadius, 1.4f);
    }

    g.setColour(Style::text());
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
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

    if (keyText.isNotEmpty())
    {
        auto labelArea = area.reduced(4, 3);
        labelArea.removeFromTop((int)(labelArea.getHeight() * 0.36f));
        g.setColour(Style::text().withAlpha(0.92f));
        g.drawFittedText(keyText, labelArea, juce::Justification::centredBottom, 1);
    }

    g.setColour(Utils::keyColourEnumToColour(layoutKey.keyColour));
    auto lightPosition = area.getX() + area.getWidth() / 2.0f;
    g.fillEllipse(lightPosition - 2.0f, area.getY() + 2.0f, 4.0f, 4.0f);

    g.setColour(Utils::zoneEnumToColour(layoutKey.zone));
    if (keyType == EigenharpKeyType::Normal) {
        g.drawRoundedRectangle(area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() - 2.0f, 5.0f, 1.0f);
    } else if (keyType == EigenharpKeyType::Perc) {
        g.drawRoundedRectangle(area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() - 2.0f, 9.0f, 1.0f);
    } else if (keyType == EigenharpKeyType::Button) {
        g.drawEllipse(area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() - 2.0f, 1.0f);
    }
}

} // namespace ecm
