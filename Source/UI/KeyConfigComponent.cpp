#include "KeyConfigComponent.h"
#include "AppStyle.h"
#include "Core/Utils.h"

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
    auto keyIsRound = keyType == EigenharpKeyType::Button;
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
    if (keyIsRound)
        g.fillEllipse(keyBounds);
    else
        g.fillRoundedRectangle(keyBounds, keyRadius);

    auto rim = keyBounds.reduced(1.5f);
    g.setColour(rimFill);
    if (keyIsRound)
        g.drawEllipse(rim, 1.0f);
    else
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
    if (keyIsRound)
        g.fillEllipse(face);
    else
        g.fillRoundedRectangle(face, keyRadius - 2.0f);

    auto gloss = face.removeFromTop(juce::jmax(2.0f, face.getHeight() * 0.22f));
    g.setColour(juce::Colour(0xffffffff).withAlpha(isSelected ? 0.18f : 0.10f));
    if (keyIsRound)
        g.fillEllipse(gloss);
    else
        g.fillRoundedRectangle(gloss, keyRadius - 2.0f);

    if (isSelected)
    {
        g.setColour(Style::accentStrong().withAlpha(0.50f));
        if (keyIsRound)
            g.drawEllipse(keyBounds.reduced(0.5f), 1.4f);
        else
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

    auto lightPosition = area.getX() + area.getWidth() / 2.0f;
    auto ledColour = Utils::keyColourEnumToColour(layoutKey.keyColour);
    g.setColour(ledColour.withAlpha(0.08f));
    g.fillEllipse(lightPosition - 5.5f, area.getY() - 0.4f, 11.0f, 11.0f);
    g.setColour(ledColour.withAlpha(0.16f));
    g.fillEllipse(lightPosition - 3.8f, area.getY() + 1.2f, 7.6f, 7.6f);
    g.setColour(ledColour);
    g.fillEllipse(lightPosition - 1.25f, area.getY() + 2.6f, 2.5f, 2.5f);

    auto zoneColour = Style::zoneColour(layoutKey.zone);
    if (layoutKey.zone != Zone::NoZone)
    {
        juce::Path zonePath;
        auto strokeBounds = area.toFloat().reduced(1.0f);

        if (keyType == EigenharpKeyType::Button)
            zonePath.addEllipse(strokeBounds);
        else if (keyType == EigenharpKeyType::Perc)
            zonePath.addRoundedRectangle(strokeBounds, 9.0f);
        else
            zonePath.addRoundedRectangle(strokeBounds, 5.0f);

        juce::PathStrokeType glowStroke(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path glowPath;
        glowStroke.createStrokedPath(glowPath, zonePath);
        g.setColour(zoneColour.withAlpha(0.14f));
        g.fillPath(glowPath);

        juce::PathStrokeType borderStroke(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path borderPath;
        borderStroke.createStrokedPath(borderPath, zonePath);

        juce::ColourGradient borderGradient(zoneColour.brighter(0.26f).withAlpha(1.0f),
                                           0.0f, strokeBounds.getY(),
                                           zoneColour.darker(0.20f).withAlpha(1.0f),
                                           0.0f, strokeBounds.getBottom(),
                                           false);
        g.setGradientFill(borderGradient);
        g.fillPath(borderPath);
    }
}

} // namespace ecm
