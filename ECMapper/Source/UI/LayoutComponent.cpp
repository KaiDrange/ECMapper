#include "LayoutComponent.h"

LayoutComponent::LayoutComponent(DeviceType deviceType, float widthFactor, float heightFactor, juce::AudioProcessorValueTreeState &pluginState) : PanelComponent(widthFactor, heightFactor), pluginState(pluginState)
{
    this->deviceType = deviceType;
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
        menu.addItem("None", [&] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::None, pluginState.state); showHidePanels(); repaint();});
        menu.addItem("Note", [&] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Note, pluginState.state); showHidePanels(); repaint();});
        menu.addItem("Chord", [&] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Chord, pluginState.state); showHidePanels(); repaint();});
        menu.addItem("Midi msg", [&] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::MidiMsg, pluginState.state); showHidePanels(); repaint();});
//        menu.addItem("Internal ctrl", [&] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Internal); showHidePanels(); repaint();});
        menu.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent(mapTypeMenuButton));
    };

    addAndMakeVisible(colourMenuButton);
    colourMenuButton.setButtonText("Colour");
    colourMenuButton.onClick = [&] {
        juce::PopupMenu menu;
        menu.addItem("None", [&] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Off, pluginState.state); repaint();});
        menu.addItem("Green", [&] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Green, pluginState.state); repaint();});
        menu.addItem("Red", [&] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Red, pluginState.state); repaint();});
        menu.addItem("Yellow", [&] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Yellow, pluginState.state); repaint();});
        menu.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent(colourMenuButton));
    };

    addAndMakeVisible(zoneMenuButton);
    zoneMenuButton.setButtonText("Zone");
    zoneMenuButton.onClick = [&] {
        juce::PopupMenu menu;
        auto zone1Item = juce::PopupMenu::Item("Zone1");
        zone1Item.setColour(zoneColours[1]);
        zone1Item.setAction([&] { LayoutWrapper::setKeyZone(activeKeyId, Zone::Zone1, pluginState.state); repaint();});
        auto zone2Item = juce::PopupMenu::Item("Zone2");
        zone2Item.setColour(zoneColours[2]);
        zone2Item.setAction([&] { LayoutWrapper::setKeyZone(activeKeyId, Zone::Zone2, pluginState.state); repaint();});
        auto zone3Item = juce::PopupMenu::Item("Zone3");
        zone3Item.setColour(zoneColours[3]);
        zone3Item.setAction([&] { LayoutWrapper::setKeyZone(activeKeyId, Zone::Zone3, pluginState.state); repaint();});
        menu.addItem("None", [&] { LayoutWrapper::setKeyZone(activeKeyId, Zone::NoZone, pluginState.state); repaint();});
        menu.addItem(zone1Item);
        menu.addItem(zone2Item);
        menu.addItem(zone3Item);
//        menu.addItem("All", [&] { LayoutWrapper::setKeyZone(activeKeyId, Zone::AllZones, pluginState.state); repaint();});
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(zoneMenuButton));
    };
    
    addAndMakeVisible(midiMessageSectionComponent);
    midiMessageSectionComponent.addListener(this);
    
    addAndMakeVisible(chordSectionComponent);
    chordSectionComponent.addListener(this);

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
    chordSectionComponent.setBounds(menuArea);
    midiMessageSectionComponent.setBounds(menuArea.removeFromTop(area.getHeight()));

    auto keyWidth = area.getWidth()/8.0;
    auto keyHeight = area.getHeight()/24.0;
    auto percKeyWidth = area.getWidth()/4.0;
    auto percKeyHeight = area.getHeight()/16.0;
    auto buttonDiameter = area.getHeight()/28.0;
    
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
    if (LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyMappingType == KeyMappingType::MidiMsg) {
        midiMessageSectionComponent.updatePanelFromMessageString(LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).mappingValue);
        midiMessageSectionComponent.setVisible(true);
        chordSectionComponent.setVisible(false);
    }
    else if (LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyMappingType == KeyMappingType::Chord) {
        chordSectionComponent.updatePanelFromMessageString(LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).mappingValue);
        midiMessageSectionComponent.setVisible(false);
        chordSectionComponent.setVisible(true);
    }
    else {
        midiMessageSectionComponent.setVisible(false);
        chordSectionComponent.setVisible(false);
    }
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

void LayoutComponent::deselectAllKeys() {
    for (int i = 0; i < getTotalKeyCount(); i++) {
        keys[i]->setToggleState(false, juce::NotificationType::dontSendNotification);
        keys[i]->setState(juce::Button::buttonNormal);
    }
    midiMessageSectionComponent.setVisible(false);
    chordSectionComponent.setVisible(false);
}

void LayoutComponent::createKeys() {
    
    for (int i = 0; i < getNormalkeyCount(); i++) {
        LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 0, .keyNo = i };
        keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Normal, pluginState));
    }

    if (deviceType == DeviceType::Pico) {
        for (int i = 0; i < getButtonCount(); i++) {
            LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 1, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Button, pluginState));
        }
    }
    else if (deviceType == DeviceType::Alpha) {
        for (int i = 0; i < getPercKeyCount(); i++) {
            LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 1, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Perc, pluginState));
        }
    }
    else { // Note: Tau course 1 (the 8 buttons) is numbered from 5 upwards
        for (int i = getPercKeyStartIndex(); i < getButtonStartIndex(); i++) {
            LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 0, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Perc, pluginState));
        }
        for (int i = 5; i < 13; i++) {
            LayoutWrapper::KeyId id = { .deviceType = deviceType, .course = 1, .keyNo = i };
            keys.push_back(new KeyConfigComponent(id, EigenharpKeyType::Button, pluginState));
        }
    }
    
    for (int i = 0; i < getTotalKeyCount(); i++) {
        addAndMakeVisible(keys[i]);
        keys[i]->setImages(keyImgNormal, keyImgOver, keyImgDown, nullptr, keyImgOn);
        keys[i]->onClick = [this, i] {
            auto selected = keys[i]->getToggleState();
            deselectAllOtherKeys(keys[i]);
            if (selected)
                activeKeyId = keys[i]->getKeyId();
            enableDisableMenuButtons(selected);
            showHidePanels();
        };
    }
}

void LayoutComponent::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    if (activeKeyId.deviceType != DeviceType::None && LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyMappingType == KeyMappingType::Note) {
        LayoutWrapper::setKeyMappingValue(activeKeyId, juce::String(midiNoteNumber), pluginState.state);
        repaint();
    }
}

void LayoutComponent::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
}

bool LayoutComponent::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) {
    if (activeKeyId.deviceType == DeviceType::None || (
               key != juce::KeyPress::leftKey &&
               key != juce::KeyPress::rightKey &&
               key != juce::KeyPress::upKey &&
               key != juce::KeyPress::downKey))
    return true;
    
    auto oldKeyIndex = -1;
    for (int i = 0; i < getTotalKeyCount(); i++) {
        if (keys[i]->getKeyId().equals(activeKeyId)) {
            oldKeyIndex = i;
            break;
        }
    }
    
    keys[oldKeyIndex]->setState(juce::Button::buttonNormal);
    int newKeyIndex = 0;

    if (LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyType == EigenharpKeyType::Normal)
        newKeyIndex = navigateNormalKeys(key, oldKeyIndex);
    else if (LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyType == EigenharpKeyType::Perc)
        newKeyIndex = navigatePercKeys(key, oldKeyIndex);
    else if (LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyType == EigenharpKeyType::Button)
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

    if (LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyType == EigenharpKeyType::Normal) {
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
    LayoutWrapper::setKeyMappingValue(activeKeyId, midiMessageSectionComponent.getMessageString(), pluginState.state);
    midiMessageSectionComponent.updatePanelFromMessageString(LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).mappingValue);
    repaint();
}

void LayoutComponent::valuesChanged(ChordSectionComponent*) {
    LayoutWrapper::setKeyMappingValue(activeKeyId, chordSectionComponent.getMessageString(), pluginState.state);
    chordSectionComponent.updatePanelFromMessageString(LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).mappingValue);
    repaint();
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
