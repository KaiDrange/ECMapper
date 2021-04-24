#include <JuceHeader.h>
#include "KeymapPanelComponent.h"

KeymapPanelComponent::KeymapPanelComponent(EigenharpMapping *eigenharpMapping, float widthFactor, float heightFactor) : PanelComponent(widthFactor, heightFactor)
{
    juce::Path p;
    p.addRoundedRectangle(0, 0, 64, 64, 10);
    keyImgNormal = new juce::DrawablePath();
    keyImgNormal->setPath(p);
    keyImgNormal->setFill(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.0f));
    keyImgNormal->setStrokeFill (juce::Colours::transparentBlack);
    keyImgNormal->setStrokeThickness(2.0f);

    keyImgOver = new juce::DrawablePath();
    keyImgOver->setPath(p);
    keyImgOver->setFill(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.8f));
    keyImgOver->setStrokeFill(juce::Colours::transparentBlack);
    keyImgOver->setStrokeThickness(2.0f);
    
    keyImgDown = new juce::DrawablePath();
    keyImgDown->setPath(p);
    keyImgDown->setFill(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    keyImgDown->setStrokeFill(juce::Colours::transparentBlack);
    keyImgDown->setStrokeThickness(2.0f);

    keyImgOn = new juce::DrawablePath();
    keyImgOn->setPath(p);
    keyImgOn->setFill(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.4f));
    keyImgOn->setStrokeFill(juce::Colours::transparentBlack);
    keyImgOn->setStrokeThickness(2.0f);

    
    this->eigenharpMapping = eigenharpMapping;
    for (int i = 0; i < eigenharpMapping->keyCount; i++) {
        keys[i] = new EigenharpKeyComponent(EigenharpKeyType::Normal);
        addAndMakeVisible(keys[i]);
        keys[i]->setImages(keyImgNormal, keyImgOver, keyImgDown, NULL, keyImgOn);
    }

    for (int i = 0; i < eigenharpMapping->percKeyCount; i++) {
        percKeys[i] = new EigenharpKeyComponent(EigenharpKeyType::Perc);
        addAndMakeVisible(percKeys[i]);
        percKeys[i]->setImages(keyImgNormal, keyImgOver, keyImgOn);
    }

    for (int i = 0; i < eigenharpMapping->buttonCount; i++) {
        buttons[i] = new EigenharpKeyComponent(EigenharpKeyType::Button);
        addAndMakeVisible(buttons[i]);
        buttons[i]->setImages(keyImgNormal, keyImgOver, keyImgOn);
    }
}

KeymapPanelComponent::~KeymapPanelComponent()
{
    for (int i = 0; i < eigenharpMapping->keyCount; i++) {
        delete keys[i];
    }
    for (int i = 0; i < eigenharpMapping->percKeyCount; i++) {
        delete percKeys[i];
    }
    for (int i = 0; i < eigenharpMapping->buttonCount; i++) {
        delete buttons[i];
    }
    
    delete keyImgNormal;
    delete keyImgOver;
    delete keyImgDown;
    delete keyImgOn;
}

void KeymapPanelComponent::resized()
{
    auto area = getLocalBounds();
    auto margin = area.getWidth()*0.02;
    area.reduce(margin, margin);
    
    auto keyWidth = area.getWidth()/8.0;
    auto keyHeight = area.getHeight()/24.0;
    auto percKeyWidth = area.getWidth()/4.0;
    auto percKeyHeight = area.getHeight()/16.0;
    auto buttonDiameter = area.getHeight()/32.0;
    
//    keyImgNormal->setSize(keyWidth, keyHeight);
    
    auto currentKeyIndex = 0;
    for (int j = 0; j < eigenharpMapping->keyRowCount; j++) {
        auto rowArea = area.removeFromLeft(keyWidth);
        for (int i = 0; i < eigenharpMapping->keyRowLengths[j]; i++) {
            keys[currentKeyIndex]->setBounds(rowArea.removeFromTop(keyHeight));
            currentKeyIndex++;
        }
    }
    
    area.removeFromLeft(margin);
    auto percRowArea = area.removeFromLeft(percKeyWidth);

    for (int i = 0; i < eigenharpMapping->percKeyCount; i++) {
        percKeys[i]->setBounds(percRowArea.removeFromTop(percKeyHeight));
    }

    percRowArea.removeFromTop(margin*2);
    auto horizontalButtonArea = percRowArea.removeFromTop(buttonDiameter);

    for (int i = 0; i < eigenharpMapping->buttonCount/2; i++) {
        buttons[i]->setBounds(horizontalButtonArea.removeFromLeft(buttonDiameter));
    }
    
    percRowArea.removeFromTop(margin*2);
    
    if (eigenharpMapping->instrumentType == InstrumentType::Tau) {
        for (int i = eigenharpMapping->buttonCount/2; i < eigenharpMapping->buttonCount; i++) {
            buttons[i]->setBounds(percRowArea.removeFromTop(buttonDiameter).removeFromLeft(buttonDiameter));
        
        }
    }
    else {
        horizontalButtonArea = percRowArea.removeFromTop(buttonDiameter);
        for (int i = eigenharpMapping->buttonCount/2; i < eigenharpMapping->buttonCount; i++) {
            buttons[i]->setBounds(horizontalButtonArea.removeFromLeft(buttonDiameter));
        }
    }
}
