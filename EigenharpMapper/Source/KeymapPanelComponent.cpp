#include <JuceHeader.h>
#include "KeymapPanelComponent.h"

KeymapPanelComponent::KeymapPanelComponent(EigenharpMapping *eigenharpMapping, float widthFactor, float heightFactor) : PanelComponent(widthFactor, heightFactor)
{
    this->eigenharpMapping = eigenharpMapping;
    for (int i = 0; i < eigenharpMapping->keyCount; i++) {
        keys[i] = new EigenharpKeyComponent(EigenharpKeyType::Normal);
        addAndMakeVisible(keys[i]);
    }

    for (int i = 0; i < eigenharpMapping->percKeyCount; i++) {
        percKeys[i] = new EigenharpKeyComponent(EigenharpKeyType::Perc);
        addAndMakeVisible(percKeys[i]);
    }

    for (int i = 0; i < eigenharpMapping->buttonCount; i++) {
        buttons[i] = new EigenharpKeyComponent(EigenharpKeyType::Button);
        addAndMakeVisible(buttons[i]);
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
