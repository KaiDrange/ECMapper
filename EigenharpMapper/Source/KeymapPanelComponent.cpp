#include <JuceHeader.h>
#include "KeymapPanelComponent.h"

KeymapPanelComponent::KeymapPanelComponent(EigenharpMapping *eigenharpMapping, float widthFactor, float heightFactor) : PanelComponent(widthFactor, heightFactor)
{
    keyImgNormal = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.0f));
    keyImgOver = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.8f));
    keyImgDown = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    keyImgOn = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.4f));
    
    this->eigenharpMapping = eigenharpMapping;
    for (int i = 0; i < eigenharpMapping->keyCount; i++) {
        keys[i] = new EigenharpKeyComponent(EigenharpKeyType::Normal, &eigenharpMapping->mappedKeys[i]);
        addAndMakeVisible(keys[i]);
        keys[i]->setImages(keyImgNormal, keyImgOver, keyImgDown, NULL, keyImgOn);
        keys[i]->id = i;
        keys[i]->onClick = [this, i] {
            auto selected = keys[i]->getToggleState();
            deselectAllOtherKeys(keys[i]);
            selectedKey = selected ? &this->eigenharpMapping->mappedKeys[i] : NULL;
            enableDisableMenuButtons(selected);
        };
    }

    for (int i = 0; i < eigenharpMapping->percKeyCount; i++) {
        percKeys[i] = new EigenharpKeyComponent(EigenharpKeyType::Perc, &eigenharpMapping->mappedPercKeys[i]);
        addAndMakeVisible(percKeys[i]);
        percKeys[i]->setImages(keyImgNormal, keyImgOver, keyImgOn);
    }

    for (int i = 0; i < eigenharpMapping->buttonCount; i++) {
        buttons[i] = new EigenharpKeyComponent(EigenharpKeyType::Button, &eigenharpMapping->mappedButtons[i]);
        addAndMakeVisible(buttons[i]);
        buttons[i]->setImages(keyImgNormal, keyImgOver, keyImgOn);
    }
    
    addAndMakeVisible (colourMenuButton);
    colourMenuButton.setButtonText("Colour");
    colourMenuButton.onClick = [&] {
        juce::PopupMenu menu;
        menu.addItem ("None", [&] { selectedKey->colour = KeyColour::Off; repaint();});
        menu.addItem ("Green", [&] { selectedKey->colour = KeyColour::Green; repaint();});
        menu.addItem ("Yellow", [&] { selectedKey->colour = KeyColour::Yellow; repaint();});
        menu.addItem ("Red", [&] { selectedKey->colour = KeyColour::Red; repaint();});
        menu.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent(colourMenuButton));
    };

    enableDisableMenuButtons(false);
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
    
    auto menuArea = area.removeFromRight(area.getWidth()*0.2);
    colourMenuButton.setBounds(menuArea.removeFromTop(area.getHeight()*0.04));
    
    
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

juce::DrawablePath* KeymapPanelComponent::createBtnImage(juce::Colour colour) {
    juce::Path p;
    p.addRoundedRectangle(0, 0, 64, 64, 10);
    juce::DrawablePath *img = new juce::DrawablePath();
    img->setPath(p);
    img->setFill(colour);
    img->setStrokeFill (juce::Colours::transparentBlack);
    img->setStrokeThickness(2.0f);
    return img;
}

void KeymapPanelComponent::enableDisableMenuButtons(bool enable) {
    colourMenuButton.setEnabled(enable);
}

void KeymapPanelComponent::deselectAllOtherKeys(const EigenharpKeyComponent *key) {
    for (int i = 0; i < eigenharpMapping->keyCount; i++) {
        if (keys[i]->id != key->id) {
            keys[i]->setToggleState(false, juce::NotificationType::dontSendNotification);
        }
    }
    
    repaint();
}

