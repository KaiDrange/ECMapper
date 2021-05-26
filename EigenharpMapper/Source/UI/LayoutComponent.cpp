#include "LayoutComponent.h"

LayoutComponent::LayoutComponent(DeviceType model, float widthFactor, float heightFactor, juce::ValueTree parentTree) : PanelComponent(widthFactor, heightFactor)
{
    deviceType = model;
    setKeyCounts(deviceType);
    keyImgNormal = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.0f));
    keyImgOver = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.8f));
    keyImgDown = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    keyImgOn = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.4f));
    
    createKeys();
    
    addAndMakeVisible(mapTypeMenuButton);
    mapTypeMenuButton.setButtonText("Type");
    
    mapTypeMenuButton.onClick = [&] {
        juce::PopupMenu menu;
        menu.addItem("None", [&] { selectedKey->setMappingType(KeyMappingType::None); showHidePanels(); repaint();});
        menu.addItem("Note", [&] { selectedKey->setMappingType(KeyMappingType::Note); showHidePanels(); repaint();});
        menu.addItem("Midi msg", [&] { selectedKey->setMappingType(KeyMappingType::MidiMsg); showHidePanels(); repaint();});
        menu.addItem("Internal ctrl", [&] { selectedKey->setMappingType(KeyMappingType::Internal); showHidePanels(); repaint();});
        menu.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent(mapTypeMenuButton));
    };

    addAndMakeVisible(colourMenuButton);
    colourMenuButton.setButtonText("Colour");
    colourMenuButton.onClick = [&] {
        juce::PopupMenu menu;
        menu.addItem("None", [&] { selectedKey->setKeyColour(KeyColour::Off); repaint();});
        menu.addItem("Green", [&] { selectedKey->setKeyColour(KeyColour::Green); repaint();});
        menu.addItem("Red", [&] { selectedKey->setKeyColour(KeyColour::Red); repaint();});
        menu.addItem("Yellow", [&] { selectedKey->setKeyColour(KeyColour::Yellow); repaint();});
        menu.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent(colourMenuButton));
    };

    addAndMakeVisible(zoneMenuButton);
    zoneMenuButton.setButtonText("Zone");
    zoneMenuButton.onClick = [&] {
        juce::PopupMenu menu;
        auto zone1Item = juce::PopupMenu::Item("Zone1");
        zone1Item.setColour(zoneColours[1]);
        zone1Item.setAction([&] { selectedKey->setZone(Zone::Zone1); repaint();});
        auto zone2Item = juce::PopupMenu::Item("Zone2");
        zone2Item.setColour(zoneColours[2]);
        zone2Item.setAction([&] { selectedKey->setZone(Zone::Zone2); repaint();});
        auto zone3Item = juce::PopupMenu::Item("Zone3");
        zone3Item.setColour(zoneColours[3]);
        zone3Item.setAction([&] { selectedKey->setZone(Zone::Zone3); repaint();});
        menu.addItem("None", [&] { selectedKey->setZone(Zone::NoZone); repaint();});
        menu.addItem(zone1Item);
        menu.addItem(zone2Item);
        menu.addItem(zone3Item);
        menu.addItem("All", [&] { selectedKey->setZone(Zone::AllZones); repaint();});
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(zoneMenuButton));
    };
    
    addAndMakeVisible(midiMessageSectionComponent);
    midiMessageSectionComponent.addListener(this);
    showHidePanels();
    enableDisableMenuButtons(false);
}

LayoutComponent::~LayoutComponent()
{
    delete keyImgNormal;
    delete keyImgOver;
    delete keyImgDown;
    delete keyImgOn;
    for (auto key : keys)
        delete key;
}

void LayoutComponent::resized()
{
    auto area = getLocalBounds();
    auto margin = area.getWidth()*0.02;
    area.reduce(margin, margin);
    
    auto menuArea = area.removeFromRight(area.getWidth()*0.4);
    mapTypeMenuButton.setBounds(menuArea.removeFromTop(area.getHeight()*0.04));
    colourMenuButton.setBounds(menuArea.removeFromTop(area.getHeight()*0.04));
    zoneMenuButton.setBounds(menuArea.removeFromTop(area.getHeight()*0.04));
    
    menuArea.removeFromTop(15);
    midiMessageSectionComponent.setBounds(menuArea.removeFromTop(area.getHeight()*0.5));
    
    auto keyWidth = area.getWidth()/8.0;
    auto keyHeight = area.getHeight()/24.0;
    auto percKeyWidth = area.getWidth()/4.0;
    auto percKeyHeight = area.getHeight()/16.0;
    auto buttonDiameter = area.getHeight()/32.0;
    
    auto currentKeyIndex = 0;
    for (int j = 0; j < getKeyRowCount(); j++) {
        auto rowArea = area.removeFromLeft(keyWidth);
        for (int i = 0; i < getKeyRowLengths()[j]; i++) {
            keys[currentKeyIndex]->setBounds(rowArea.removeFromTop(keyHeight));
            currentKeyIndex++;
        }
    }
    
    area.removeFromLeft(margin);
    auto percRowArea = area.removeFromLeft(percKeyWidth*2);

    for (int i = getPercKeyStartIndex(); i < getButtonStartIndex(); i++) {
        keys[i]->setBounds(percRowArea.removeFromTop(percKeyHeight).removeFromLeft(percRowArea.getWidth()/2));
    }

    percRowArea.removeFromTop(margin*2);
    auto horizontalButtonArea = percRowArea.removeFromTop(buttonDiameter);

    for (int i = getButtonStartIndex(); i < getButtonStartIndex() + getButtonCount()/2; i++) {
        keys[i]->setBounds(horizontalButtonArea.removeFromLeft(buttonDiameter));
    }
    
    percRowArea.removeFromTop(margin*2);
    
    if (deviceType == DeviceType::Tau) {
        for (int i = getButtonStartIndex() + getButtonCount()/2; i < getTotalKeyCount(); i++) {
            keys[i]->setBounds(percRowArea.removeFromTop(buttonDiameter).removeFromLeft(buttonDiameter));

        }
    }
    else {
        horizontalButtonArea = percRowArea.removeFromTop(buttonDiameter);
        for (int i = getButtonStartIndex() + getButtonCount()/2; i < getTotalKeyCount(); i++) {
            keys[i]->setBounds(horizontalButtonArea.removeFromLeft(buttonDiameter));
        }
    }
}

juce::DrawablePath* LayoutComponent::createBtnImage(juce::Colour colour) {
    juce::Path p;
    p.addRoundedRectangle(0, 0, 64, 64, 10);
    juce::DrawablePath *img = new juce::DrawablePath();
    img->setPath(p);
    img->setFill(colour);
    img->setStrokeFill (juce::Colours::transparentBlack);
    img->setStrokeThickness(2.0f);
    return img;
}

void LayoutComponent::enableDisableMenuButtons(bool enable) {
    colourMenuButton.setEnabled(enable);
    zoneMenuButton.setEnabled(enable);
    mapTypeMenuButton.setEnabled(enable);
}

void LayoutComponent::showHidePanels() {
    if (selectedKey != nullptr && selectedKey->getMappingType() == KeyMappingType::MidiMsg) {
        midiMessageSectionComponent.updatePanelFromMessageString(selectedKey->getMappingValue());
        midiMessageSectionComponent.setVisible(true);
    }
    else
        midiMessageSectionComponent.setVisible(false);
}

void LayoutComponent::deselectAllOtherKeys(const KeyConfigComponent *key) {
    auto keyId = key->getKeyId();
    for (int i = 0; i < getTotalKeyCount(); i++) {
        if (!keys[i]->getKeyId().equals(keyId)) {
            keys[i]->setToggleState(false, juce::NotificationType::dontSendNotification);
            keys[i]->setState(juce::Button::buttonNormal);
        }
    }
}

void LayoutComponent::createKeys() {
    
    for (int i = 0; i < getNormalkeyCount(); i++) {
        LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 0, .keyNo = i };
//        EigenharpKeyComponent *key = new EigenharpKeyComponent(id, EigenharpKeyType::Normal, layout.valueTree);
        keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Normal, layout.valueTree));
    }

    if (deviceType == DeviceType::Pico) {
        for (int i = 0; i < getButtonCount(); i++) {
            KeyConfig::KeyId id = { .course = 1, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Button, layout.valueTree));
        }
    }
    else {
        for (int i = 0; i < getPercKeyCount(); i++) {
            LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 1, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Perc, layout.valueTree));
        }
        for (int i = 0; i < getButtonCount(); i++) {
            KeyConfig::KeyId id = { .course = 2, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Button, layout.valueTree));
        }
    }
    
    for (int i = 0; i < getTotalKeyCount(); i++) {
        addAndMakeVisible(keys[i]);
        keys[i]->setImages(keyImgNormal, keyImgOver, keyImgDown, nullptr, keyImgOn);
        keys[i]->onClick = [this, i] {
            auto selected = keys[i]->getToggleState();
            deselectAllOtherKeys(keys[i]);
            selectedKey = selected ? &keys[i]->keyConfig : nullptr;
            enableDisableMenuButtons(selected);
            showHidePanels();
        };
    }
}

void LayoutComponent::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    if (selectedKey != nullptr && selectedKey->getMappingType() == KeyMappingType::Note) {
        selectedKey->setMappingValue(juce::String(midiNoteNumber));
        repaint();
    }
}

void LayoutComponent::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
}

bool LayoutComponent::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) {
    if (selectedKey == nullptr || (
               key != juce::KeyPress::leftKey &&
               key != juce::KeyPress::rightKey &&
               key != juce::KeyPress::upKey &&
               key != juce::KeyPress::downKey))
    return true;
    
    auto oldKeyIndex = -1;
    auto keyId = selectedKey->getKeyId();
    for (int i = 0; i < getTotalKeyCount(); i++) {
        if (keys[i]->getKeyId().equals(keyId)) {
            oldKeyIndex = i;
            break;
        }
    }
    
    keys[oldKeyIndex]->setState(juce::Button::buttonNormal);
    int newKeyIndex = 0;

    if (selectedKey->getKeyType() == EigenharpKeyType::Normal)
        newKeyIndex = navigateNormalKeys(key, oldKeyIndex);
    else if (selectedKey->getKeyType() == EigenharpKeyType::Perc)
        newKeyIndex = navigatePercKeys(key, oldKeyIndex);
    else if (selectedKey->getKeyType() == EigenharpKeyType::Button)
        newKeyIndex = navigateButtons(key, oldKeyIndex);

    if (newKeyIndex != oldKeyIndex)
        keys[newKeyIndex]->triggerClick();
    return true;
}

int LayoutComponent::navigateNormalKeys(const juce::KeyPress &key, int selectedKeyIndex) {
    const int *rowLengths = getKeyRowLengths();

    int rowStartIndexes[5] = {
        0,
        rowLengths[0],
        rowLengths[0] + rowLengths[1],
        rowLengths[0] + rowLengths[1] + rowLengths[2],
        rowLengths[0] + rowLengths[1] + rowLengths[2] + rowLengths[3]
    };

    int rowNumber = getRowNumber(selectedKeyIndex);

    if (selectedKey->getKeyType() == EigenharpKeyType::Normal) {
        if (key == juce::KeyPress::upKey) {
            selectedKeyIndex--;
            if (selectedKeyIndex == rowStartIndexes[rowNumber] - 1)
                selectedKeyIndex += rowLengths[rowNumber];
        }
        else if (key == juce::KeyPress::downKey) {
            selectedKeyIndex++;
            if (selectedKeyIndex >= getPercKeyStartIndex() ||
                    selectedKeyIndex == rowStartIndexes[rowNumber+1])
                selectedKeyIndex -= rowLengths[rowNumber];
        }
        else if (key == juce::KeyPress::leftKey) {
            if (rowNumber == 0)
                selectedKeyIndex += rowStartIndexes[getKeyRowCount()-1];
            else if (deviceType == DeviceType::Tau &&
                     selectedKeyIndex >= rowStartIndexes[3]-4 && selectedKeyIndex < rowStartIndexes[3]) {
                selectedKeyIndex += rowLengths[3];
            }
            else
                selectedKeyIndex -= rowLengths[rowNumber-1];
        }
        else if (key == juce::KeyPress::rightKey) {
            if (deviceType == DeviceType::Tau &&
                     selectedKeyIndex >= rowStartIndexes[4]-4 && selectedKeyIndex < rowStartIndexes[4]) {
                selectedKeyIndex -= rowLengths[3];
            }
            else if (rowNumber == getKeyRowCount()-1)
                selectedKeyIndex -= rowStartIndexes[rowNumber];
            else
                selectedKeyIndex += rowLengths[rowNumber];
        }
    }
    return selectedKeyIndex;
}

int LayoutComponent::navigatePercKeys(const juce::KeyPress &key, int selectedKeyIndex) {
    if (key == juce::KeyPress::upKey) {
        selectedKeyIndex--;
        if (selectedKeyIndex == getPercKeyStartIndex()-1)
            selectedKeyIndex += getPercKeyCount();
    }
    else if (key == juce::KeyPress::downKey) {
        selectedKeyIndex++;
        if (selectedKeyIndex >= getButtonStartIndex())
            selectedKeyIndex -= getPercKeyCount();
    }
    
    return selectedKeyIndex;
}

int LayoutComponent::navigateButtons(const juce::KeyPress &key, int selectedKeyIndex) {
    
    return selectedKeyIndex;
}


int LayoutComponent::getRowNumber(int keyIndex) {
    const int *rowLengths = getKeyRowLengths();
    int row = 0;
    int counter = rowLengths[row];
    while (keyIndex >= counter) {
        row++;
        counter += rowLengths[row];
    }
    return row;
}

void LayoutComponent::valuesChanged(MidiMessageSectionComponent*) {
    selectedKey->setMappingValue(midiMessageSectionComponent.getMessageString());
}

int LayoutComponent::getNormalkeyCount() const {
    return normalKeyCount;
}

int LayoutComponent::getPercKeyCount() const {
    return percKeyCount;
}

int LayoutComponent::getButtonCount() const {
    return buttonCount;
}

int LayoutComponent::getStripCount() const {
    return stripCount;
}

int LayoutComponent::getKeyRowCount() const {
    return keyRowCount;
}

const int* LayoutComponent::getKeyRowLengths() const {
    return keyRowLengths;
}

int LayoutComponent::getTotalKeyCount() const {
    return normalKeyCount + percKeyCount + buttonCount;
}

int LayoutComponent::getPercKeyStartIndex() const {
    return normalKeyCount;
}

int LayoutComponent::getButtonStartIndex() const {
    return normalKeyCount + percKeyCount;
}

void LayoutComponent::setKeyCounts(DeviceType deviceType) {
    switch(deviceType) {
        case DeviceType::Alpha:
            normalKeyCount = 120;
            percKeyCount = 12;
            keyRowCount = 5;
            keyRowLengths[0] = 24;
            keyRowLengths[1] = 24;
            keyRowLengths[2] = 24;
            keyRowLengths[3] = 24;
            keyRowLengths[4] = 24;
            buttonCount = 0;
            stripCount = 2;
            break;
        case DeviceType::Tau:
            normalKeyCount = 72;
            percKeyCount = 12;
            keyRowCount = 4;
            keyRowLengths[0] = 16;
            keyRowLengths[1] = 16;
            keyRowLengths[2] = 20;
            keyRowLengths[3] = 20;
            keyRowLengths[4] = 0;
            buttonCount = 8;
            stripCount = 1;
            break;
        case DeviceType::Pico:
            normalKeyCount = 18;
            percKeyCount = 0;
            keyRowCount = 2;
            keyRowLengths[0] = 9;
            keyRowLengths[1] = 9;
            keyRowLengths[2] = 0;
            keyRowLengths[3] = 0;
            keyRowLengths[4] = 0;
            buttonCount = 4;
            stripCount = 1;
            break;
        default:
            break;
    }
}
