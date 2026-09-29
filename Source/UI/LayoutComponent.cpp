#include "LayoutComponent.h"

#include "AppStyle.h"

namespace ecm {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wshadow-field"
#endif
LayoutComponent::LayoutComponent(InstrumentType deviceType, float widthFactor, float heightFactor, juce::AudioProcessorValueTreeState& pluginState) 
    : PanelComponent(widthFactor, heightFactor), 
      deviceType(deviceType), 
      pluginState(pluginState) {
    
    setKeyCounts(deviceType);
    keyImgNormal = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.0f));
    keyImgOver = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.8f));
    keyImgDown = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 1.0f));
    keyImgOn = createBtnImage(juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, 0.4f));
    
    createKeys();
    
    addAndMakeVisible(mapTypeMenuButton);
    mapTypeMenuButton.onClick = [this] {
        juce::PopupMenu menu;
        menu.addItem("None", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::None, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("Note", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Note, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("Palm mute", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::PalmMute, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("Strum", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Strum, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("Chord", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Chord, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("Midi msg", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::MidiMsg, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("App Ctrl", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::AppCtrl, this->pluginState.state); showHidePanels(); repaint(); });
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(mapTypeMenuButton));
    };

    addChildComponent(palmMuteModeSelector);
    palmMuteModeSelector.addItem("Mode: Latch", 1);
    palmMuteModeSelector.addItem("Mode: Momentary", 2);
    palmMuteModeSelector.setTooltip("Latch toggles palm mute on each press. Momentary mutes while held. Affects this device's selected zone.");
    palmMuteModeSelector.onChange = [this] {
        if (activeKeyId.deviceType == InstrumentType::None) return;
        LayoutWrapper::setKeyMappingValue(activeKeyId, palmMuteModeSelector.getSelectedId() == 2
            ? "PalmMute;Momentary" : "PalmMute;Latch", this->pluginState.state);
    };

    addAndMakeVisible(stringSelector);
    stringSelector.addItem("String: None", 1);
    for (int number = 1; number <= 12; ++number)
        stringSelector.addItem("String: " + juce::String(number), number + 1);
    stringSelector.setTooltip("Assign this fret to a string. Held strummed notes follow fret changes using pitch bend; these note keys play independently.");
    stringSelector.onChange = [this] {
        if (activeKeyId.deviceType != InstrumentType::None)
            LayoutWrapper::setKeyStringNumber(activeKeyId, stringSelector.getSelectedId() - 1, this->pluginState.state);
        showHidePanels();
    };

    addChildComponent(openStringNote.label);
    addChildComponent(openStringNote.setButton);
    addChildComponent(openStringNote.clearButton);
    openStringNote.setButton.setClickingTogglesState(true);
    openStringNote.setButton.setTooltip("Click Set, then choose a note on the keyboard for this strum key.");
    openStringNote.clearButton.onClick = [this] {
        openStringNote.setButton.setToggleState(false, juce::dontSendNotification);
        setOpenStringNote(-1);
    };
    openStringNote.label.setTooltip("Played by this strum key when no linked source note is held.");
    addChildComponent(stringMidiChannelSelector);
    stringMidiChannelSelector.addItem("Force MIDI Channel: Off", 1);
    for (int channel = 1; channel <= 16; ++channel)
        stringMidiChannelSelector.addItem("Force MIDI Channel: " + juce::String(channel), channel + 1);
    stringMidiChannelSelector.setTooltip("Overrides the output zone channel/MPE routing for this strum key.");
    stringMidiChannelSelector.onChange = [this] {
        if (activeKeyId.deviceType == InstrumentType::None) return;
        auto settings = LayoutWrapper::getStrumSettings(activeKeyId, this->pluginState.state);
        settings.midiChannel = stringMidiChannelSelector.getSelectedId() - 1;
        LayoutWrapper::setStrumSettings(activeKeyId, settings, this->pluginState.state);
    };

    addChildComponent(strumSourceZoneSelector);
    addChildComponent(strumSourceStringSelector);
    addChildComponent(strumNoteOffToggle);
    strumNoteOffToggle.setTooltip("On: release notes with this strum key. Off: fretted strums release with the last linked note key; open strums sustain until palm-muted or replaced by another strum.");
    strumNoteOffToggle.onClick = [this] {
        if (activeKeyId.deviceType == InstrumentType::None) return;
        auto settings = LayoutWrapper::getStrumSettings(activeKeyId, this->pluginState.state);
        settings.controlsNoteOff = strumNoteOffToggle.getToggleState();
        LayoutWrapper::setStrumSettings(activeKeyId, settings, this->pluginState.state);
    };
    addChildComponent(strumExpressionToggle);
    strumExpressionToggle.setTooltip("On: this strum key controls roll, yaw and pressure. Off: the linked source key controls them.");
    for (int zone = 1; zone <= 3; ++zone)
        strumSourceZoneSelector.addItem("Linked zone: " + juce::String(zone), zone);
    for (int string = 1; string <= 12; ++string)
        strumSourceStringSelector.addItem("Linked string: " + juce::String(string), string);
    strumSourceZoneSelector.setTooltip("Read the held note or chord from this zone on the same device");
    strumSourceStringSelector.setTooltip("Use this note string, or the matching ascending pitch from held chords");
    auto updateStrumLink = [this] {
        if (activeKeyId.deviceType == InstrumentType::None) return;
        LayoutWrapper::setKeyMappingValue(activeKeyId,
            "Strum;" + juce::String(strumSourceZoneSelector.getSelectedId()) + ";"
                + juce::String(strumSourceStringSelector.getSelectedId()) + ";"
                + juce::String(strumExpressionToggle.getToggleState() ? 1 : 0), this->pluginState.state);
        repaint();
    };
    strumSourceZoneSelector.onChange = updateStrumLink;
    strumSourceStringSelector.onChange = updateStrumLink;
    strumExpressionToggle.onClick = updateStrumLink;

    addAndMakeVisible(colourMenuButton);
    colourMenuButton.onClick = [this] {
        juce::PopupMenu menu;
        menu.addItem("None", [this] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Off, this->pluginState.state); repaint(); });
        menu.addItem("Green", [this] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Green, this->pluginState.state); repaint(); });
        menu.addItem("Red", [this] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Red, this->pluginState.state); repaint(); });
        menu.addItem("Yellow", [this] { LayoutWrapper::setKeyColour(activeKeyId, KeyColour::Yellow, this->pluginState.state); repaint(); });
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(colourMenuButton));
    };

    addAndMakeVisible(zoneMenuButton);
    zoneMenuButton.onClick = [this] {
        juce::PopupMenu menu;
        auto addItem = [&](const juce::String& name, Zone zone, juce::Colour col) {
            juce::PopupMenu::Item item(name);
            item.setColour(col);
            item.setAction([this, zone] { LayoutWrapper::setKeyZone(activeKeyId, zone, this->pluginState.state); showHidePanels(); repaint(); });
            menu.addItem(item);
        };
        menu.addItem("None", [this] { LayoutWrapper::setKeyZone(activeKeyId, Zone::NoZone, this->pluginState.state); showHidePanels(); repaint(); });
        addItem("Zone1", Zone::Zone1, Style::zoneColour(Zone::Zone1));
        addItem("Zone2", Zone::Zone2, Style::zoneColour(Zone::Zone2));
        addItem("Zone3", Zone::Zone3, Style::zoneColour(Zone::Zone3));
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(zoneMenuButton));
    };
    
    addAndMakeVisible(midiMessageSectionComponent);
    midiMessageSectionComponent.addListener(this);
    
    addAndMakeVisible(chordSectionComponent);
    chordSectionComponent.addListener(this);

    addAndMakeVisible(appCtrlSectionComponent);
    appCtrlSectionComponent.addListener(this);

    showHidePanels();
    enableDisableMenuButtons(false);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

LayoutComponent::~LayoutComponent() = default;

void LayoutComponent::resized() {
    auto area = getLocalBounds();
    const auto areaWidth = static_cast<float>(area.getWidth());
    const auto areaHeight = static_cast<float>(area.getHeight());
    auto margin = areaWidth * 0.02f;
    area.reduce(static_cast<int>(margin), static_cast<int>(margin));
    
    auto menuArea = area.removeFromRight(static_cast<int>(areaWidth * 0.4f));
    mapTypeMenuButton.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    colourMenuButton.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    zoneMenuButton.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    
    if (palmMuteModeSelector.isVisible()) {
        menuArea.removeFromTop(8);
        palmMuteModeSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    }
    if (stringSelector.isVisible()) {
        menuArea.removeFromTop(8);
        stringSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    }

    if (strumSourceZoneSelector.isVisible()) {
        menuArea.removeFromTop(8);
        strumSourceZoneSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
        menuArea.removeFromTop(8);
        strumSourceStringSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
        menuArea.removeFromTop(8);
        strumExpressionToggle.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
        strumNoteOffToggle.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    }
    if (openStringNote.label.isVisible()) {
        menuArea.removeFromTop(8);
        openStringNote.label.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
        auto noteButtons = menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f));
        openStringNote.setButton.setBounds(noteButtons.removeFromLeft(noteButtons.getWidth() / 2));
        openStringNote.clearButton.setBounds(noteButtons);
        menuArea.removeFromTop(8);
        stringMidiChannelSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
    }
    menuArea.removeFromTop(15);
    chordSectionComponent.setBounds(menuArea);
    appCtrlSectionComponent.setBounds(menuArea);
    midiMessageSectionComponent.setBounds(menuArea.removeFromTop(area.getHeight()));

    const auto innerWidth = static_cast<float>(area.getWidth());
    const auto innerHeight = static_cast<float>(area.getHeight());
    auto keyWidth = innerWidth / 8.0f;
    auto keyHeight = innerHeight / 24.0f;
    auto percKeyWidth = innerWidth / 4.0f;
    auto percKeyHeight = innerHeight / 16.0f;
    auto buttonDiameter = innerHeight / 28.0f;
    
    int currentKeyIndex = 0;
    for (int j = 0; j < getKeyRowCount(); j++) {
        auto rowArea = area.removeFromLeft(static_cast<int>(keyWidth));
        for (int i = 0; i < getKeyRowLengths()[j]; i++) {
            keys[currentKeyIndex]->setBounds(rowArea.removeFromTop(static_cast<int>(keyHeight)));
            currentKeyIndex++;
        }
    }
    
    area.removeFromLeft(static_cast<int>(margin));
    auto percRowArea = area.removeFromLeft(static_cast<int>(percKeyWidth * 2));

    for (int i = getPercKeyStartIndex(); i < getButtonStartIndex(); i++) {
        keys[i]->setBounds(percRowArea.removeFromTop(static_cast<int>(percKeyHeight)).removeFromLeft(percRowArea.getWidth() / 2));
    }

    percRowArea.removeFromTop(static_cast<int>(margin * 2));
    auto horizontalButtonArea = percRowArea.removeFromTop(static_cast<int>(buttonDiameter));

    for (int i = getButtonStartIndex(); i < getButtonStartIndex() + getButtonCount() / 2; i++) {
        keys[i]->setBounds(horizontalButtonArea.removeFromLeft(static_cast<int>(buttonDiameter)));
    }
    
    percRowArea.removeFromTop(static_cast<int>(margin * 2));
    
    if (deviceType == InstrumentType::Tau) {
        for (int i = getButtonStartIndex() + getButtonCount() / 2; i < getTotalKeyCount(); i++) {
            keys[i]->setBounds(percRowArea.removeFromTop(static_cast<int>(buttonDiameter)).removeFromLeft(static_cast<int>(buttonDiameter)));
        }
    } else {
        horizontalButtonArea = percRowArea.removeFromTop(static_cast<int>(buttonDiameter));
        for (int i = getButtonStartIndex() + getButtonCount() / 2; i < getTotalKeyCount(); i++) {
            keys[i]->setBounds(horizontalButtonArea.removeFromLeft(static_cast<int>(buttonDiameter)));
        }
    }
}

std::unique_ptr<juce::DrawablePath> LayoutComponent::createBtnImage(juce::Colour colour) {
    juce::Path p;
    p.addRoundedRectangle(0, 0, 64, 64, 10);
    auto img = std::make_unique<juce::DrawablePath>();
    img->setPath(p);
    img->setFill(colour);
    img->setStrokeFill(juce::Colours::transparentBlack);
    img->setStrokeThickness(2.0f);
    return img;
}

void LayoutComponent::enableDisableMenuButtons(bool enable) {
    colourMenuButton.setEnabled(enable);
    zoneMenuButton.setEnabled(enable);
    mapTypeMenuButton.setEnabled(enable);
    stringSelector.setEnabled(enable);
    palmMuteModeSelector.setEnabled(enable);
    openStringNote.label.setEnabled(enable);
    openStringNote.setButton.setEnabled(enable);
    openStringNote.clearButton.setEnabled(enable);
    stringMidiChannelSelector.setEnabled(enable);
    strumSourceZoneSelector.setEnabled(enable);
    strumSourceStringSelector.setEnabled(enable);
    strumExpressionToggle.setEnabled(enable);
    strumNoteOffToggle.setEnabled(enable);
}

void LayoutComponent::showHidePanels() {    
    auto layoutKey = LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state);
    const bool strum = layoutKey.keyMappingType == KeyMappingType::Strum;
    palmMuteModeSelector.setVisible(layoutKey.keyMappingType == KeyMappingType::PalmMute);
    palmMuteModeSelector.setSelectedId(layoutKey.mappingValue == "PalmMute;Momentary" ? 2 : 1, juce::dontSendNotification);
    strumSourceZoneSelector.setVisible(strum);
    strumSourceStringSelector.setVisible(strum);
    strumExpressionToggle.setVisible(strum);
    strumNoteOffToggle.setVisible(strum);
    zoneMenuButton.setButtonText(strum ? "Output zone" : "Zone");
    if (strum) {
        auto parts = juce::StringArray::fromTokens(layoutKey.mappingValue, ";", "");
        const bool valid = (parts.size() == 3 || parts.size() == 4) && parts[0] == "Strum";
        strumExpressionToggle.setToggleState(!valid || parts.size() == 3 || parts[3].getIntValue() != 0, juce::dontSendNotification);
        strumSourceZoneSelector.setSelectedId(valid ? juce::jlimit(1, 3, parts[1].getIntValue()) : 1, juce::dontSendNotification);
        strumSourceStringSelector.setSelectedId(valid ? juce::jlimit(1, 12, parts[2].getIntValue()) : 1, juce::dontSendNotification);
    }
    stringSelector.setSelectedId(layoutKey.stringNumber + 1, juce::dontSendNotification);
    stringSelector.setVisible(layoutKey.keyMappingType == KeyMappingType::Note);
    openStringNote.label.setVisible(strum);
    openStringNote.setButton.setVisible(strum);
    openStringNote.clearButton.setVisible(strum);
    openStringNote.setButton.setToggleState(false, juce::dontSendNotification);
    stringMidiChannelSelector.setVisible(strum);
    const bool canEditStrum = strumSourceZoneSelector.isEnabled();
    openStringNote.label.setEnabled(canEditStrum);
    openStringNote.setButton.setEnabled(canEditStrum);
    openStringNote.clearButton.setEnabled(canEditStrum);
    stringMidiChannelSelector.setEnabled(canEditStrum);
    const auto settings = LayoutWrapper::getStrumSettings(activeKeyId, pluginState.state);
    strumNoteOffToggle.setToggleState(settings.controlsNoteOff, juce::dontSendNotification);
    openStringNote.midiNoteNumber = settings.openNote;
    updateOpenStringNoteLabel();
    stringMidiChannelSelector.setSelectedId(settings.midiChannel + 1, juce::dontSendNotification);
    resized();
    if (layoutKey.keyMappingType == KeyMappingType::MidiMsg) {
        midiMessageSectionComponent.updatePanelFromMessageString(layoutKey.mappingValue);
        midiMessageSectionComponent.setVisible(true);
        chordSectionComponent.setVisible(false);
        appCtrlSectionComponent.setVisible(false);
    } else if (layoutKey.keyMappingType == KeyMappingType::Chord) {
        chordSectionComponent.updatePanelFromMessageString(layoutKey.mappingValue);
        midiMessageSectionComponent.setVisible(false);
        chordSectionComponent.setVisible(true);
        appCtrlSectionComponent.setVisible(false);
    } else if (layoutKey.keyMappingType == KeyMappingType::AppCtrl) {
        appCtrlSectionComponent.updatePanelFromMessageString(layoutKey.mappingValue);
        midiMessageSectionComponent.setVisible(false);
        chordSectionComponent.setVisible(false);
        appCtrlSectionComponent.setVisible(true);
    } else {
        midiMessageSectionComponent.setVisible(false);
        chordSectionComponent.setVisible(false);
        appCtrlSectionComponent.setVisible(false);
    }
}

void LayoutComponent::deselectAllOtherKeys(const KeyConfigComponent* key) {
    auto keyId = key->getKeyId();
    for (auto* k : keys) {
        if (!k->getKeyId().equals(keyId)) {
            k->setToggleState(false, juce::dontSendNotification);
            k->setState(juce::Button::buttonNormal);
        }
    }
}

void LayoutComponent::deselectAllKeys() {
    stringSelector.setVisible(false);
    palmMuteModeSelector.setVisible(false);
    openStringNote.label.setVisible(false);
    openStringNote.setButton.setVisible(false);
    openStringNote.clearButton.setVisible(false);
    openStringNote.setButton.setToggleState(false, juce::dontSendNotification);
    stringMidiChannelSelector.setVisible(false);
    strumSourceZoneSelector.setVisible(false);
    strumSourceStringSelector.setVisible(false);
    strumExpressionToggle.setVisible(false);
    strumNoteOffToggle.setVisible(false);
    for (auto* k : keys) {
        k->setToggleState(false, juce::dontSendNotification);
        k->setState(juce::Button::buttonNormal);
    }
    midiMessageSectionComponent.setVisible(false);
    chordSectionComponent.setVisible(false);
    appCtrlSectionComponent.setVisible(false);
}

void LayoutComponent::createKeys() {
    for (int i = 0; i < getNormalkeyCount(); i++) {
        LayoutWrapper::KeyId id = { .course = 0, .keyNo = i, .deviceType = deviceType };
        keys.add(new KeyConfigComponent(id, EigenharpKeyType::Normal, pluginState));
    }

    if (deviceType == InstrumentType::Pico) {
        for (int i = 0; i < getButtonCount(); i++) {
            LayoutWrapper::KeyId id = { .course = 1, .keyNo = i, .deviceType = deviceType };
            keys.add(new KeyConfigComponent(id, EigenharpKeyType::Button, pluginState));
        }
    } else if (deviceType == InstrumentType::Alpha) {
        for (int i = 0; i < getPercKeyCount(); i++) {
            LayoutWrapper::KeyId id = { .course = 1, .keyNo = i, .deviceType = deviceType };
            keys.add(new KeyConfigComponent(id, EigenharpKeyType::Perc, pluginState));
        }
    } else { // Tau
        for (int i = 0; i < getPercKeyCount(); i++) {
            LayoutWrapper::KeyId id = { .course = 1, .keyNo = i, .deviceType = deviceType };
            keys.add(new KeyConfigComponent(id, EigenharpKeyType::Perc, pluginState));
        }
        for (int i = 0; i < getButtonCount(); i++) {
            LayoutWrapper::KeyId id = { .course = 2, .keyNo = i, .deviceType = deviceType };
            keys.add(new KeyConfigComponent(id, EigenharpKeyType::Button, pluginState));
        }
    }
    
    for (auto* k : keys) {
        addAndMakeVisible(k);
        k->setImages(keyImgNormal.get(), keyImgOver.get(), keyImgDown.get(), nullptr, keyImgOn.get());
        k->onClick = [this, k] {
            auto selected = k->getToggleState();
            deselectAllOtherKeys(k);
            if (selected) activeKeyId = k->getKeyId();
            enableDisableMenuButtons(selected);
            showHidePanels();
        };
    }
}

void LayoutComponent::updateOpenStringNoteLabel() {
    openStringNote.label.setText("Open string note: " + (openStringNote.midiNoteNumber >= 0
        ? juce::MidiMessage::getMidiNoteName(openStringNote.midiNoteNumber, true, true, 3)
        : juce::String("None")), juce::dontSendNotification);
}

void LayoutComponent::setOpenStringNote(int midiNoteNumber) {
    if (activeKeyId.deviceType == InstrumentType::None) return;
    const auto key = LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state);
    if (key.keyMappingType != KeyMappingType::Strum) return;
    auto settings = LayoutWrapper::getStrumSettings(activeKeyId, pluginState.state);
    settings.openNote = midiNoteNumber;
    LayoutWrapper::setStrumSettings(activeKeyId, settings, pluginState.state);
    openStringNote.midiNoteNumber = midiNoteNumber;
    updateOpenStringNoteLabel();
}

void LayoutComponent::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float) {
    if (openStringNote.setButton.isVisible() && openStringNote.setButton.isEnabled()
        && openStringNote.setButton.getToggleState()) {
        setOpenStringNote(midiNoteNumber);
        return;
    }
    if (activeKeyId.deviceType != InstrumentType::None && LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).keyMappingType == KeyMappingType::Note) {
        LayoutWrapper::setKeyMappingValue(activeKeyId, juce::String(midiNoteNumber), pluginState.state);
        repaint();
    }
}

void LayoutComponent::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
    openStringNote.setButton.setToggleState(false, juce::dontSendNotification);
}

bool LayoutComponent::keyPressed(const juce::KeyPress& key, juce::Component*) {
    if (activeKeyId.deviceType == InstrumentType::None) return true;
    if (key != juce::KeyPress::leftKey && key != juce::KeyPress::rightKey && key != juce::KeyPress::upKey && key != juce::KeyPress::downKey) return true;
    
    int oldKeyIndex = -1;
    for (int i = 0; i < (int)keys.size(); i++) {
        if (keys[i]->getKeyId().equals(activeKeyId)) {
            oldKeyIndex = i;
            break;
        }
    }
    if (oldKeyIndex == -1) return true;
    
    keys[oldKeyIndex]->setState(juce::Button::buttonNormal);
    int newKeyIndex = 0;

    auto layoutKey = LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state);
    if (layoutKey.keyType == EigenharpKeyType::Normal) newKeyIndex = navigateNormalKeys(key, oldKeyIndex);
    else if (layoutKey.keyType == EigenharpKeyType::Perc) newKeyIndex = navigatePercKeys(key, oldKeyIndex);
    else if (layoutKey.keyType == EigenharpKeyType::Button) newKeyIndex = navigateButtons(key, oldKeyIndex);

    if (newKeyIndex != oldKeyIndex) keys[newKeyIndex]->triggerClick();
    return true;
}

int LayoutComponent::navigateNormalKeys(const juce::KeyPress& key, int selectedKeyIndex) {
    const int* rowLengths = getKeyRowLengths();
    int rowStartIndexes[6] = { 0, rowLengths[0], rowLengths[0]+rowLengths[1], rowLengths[0]+rowLengths[1]+rowLengths[2], rowLengths[0]+rowLengths[1]+rowLengths[2]+rowLengths[3], getPercKeyStartIndex() };
    int rowNumber = getRowNumber(selectedKeyIndex);

    if (key == juce::KeyPress::upKey) {
        selectedKeyIndex--;
        if (selectedKeyIndex < rowStartIndexes[rowNumber]) selectedKeyIndex += rowLengths[rowNumber];
    } else if (key == juce::KeyPress::downKey) {
        selectedKeyIndex++;
        if (selectedKeyIndex >= rowStartIndexes[rowNumber+1]) selectedKeyIndex -= rowLengths[rowNumber];
    } else if (key == juce::KeyPress::leftKey) {
        if (rowNumber > 0) selectedKeyIndex -= rowLengths[rowNumber-1];
        else selectedKeyIndex += rowStartIndexes[getKeyRowCount()-1];
    } else if (key == juce::KeyPress::rightKey) {
        if (rowNumber < getKeyRowCount()-1) selectedKeyIndex += rowLengths[rowNumber];
        else selectedKeyIndex -= rowStartIndexes[rowNumber];
    }
    return selectedKeyIndex;
}

int LayoutComponent::navigatePercKeys(const juce::KeyPress& key, int selectedKeyIndex) {
    if (key == juce::KeyPress::upKey) {
        selectedKeyIndex--;
        if (selectedKeyIndex < getPercKeyStartIndex()) selectedKeyIndex += getPercKeyCount();
    } else if (key == juce::KeyPress::downKey) {
        selectedKeyIndex++;
        if (selectedKeyIndex >= getButtonStartIndex()) selectedKeyIndex -= getPercKeyCount();
    }
    return selectedKeyIndex;
}

int LayoutComponent::navigateButtons(const juce::KeyPress&, int selectedKeyIndex) {
    return selectedKeyIndex;
}

int LayoutComponent::getRowNumber(int keyIndex) {
    const int* rowLengths = getKeyRowLengths();
    int row = 0;
    int counter = rowLengths[0];
    while (keyIndex >= counter && row < getKeyRowCount() - 1) {
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

void LayoutComponent::valuesChanged(AppCtrlSectionComponent*) {
    LayoutWrapper::setKeyMappingValue(activeKeyId, appCtrlSectionComponent.getMessageString(), pluginState.state);
    appCtrlSectionComponent.updatePanelFromMessageString(LayoutWrapper::getLayoutKey(activeKeyId, pluginState.state).mappingValue);
    repaint();
}

int LayoutComponent::getNormalkeyCount() const { return normalKeyCount; }
int LayoutComponent::getPercKeyCount() const { return percKeyCount; }
int LayoutComponent::getButtonCount() const { return buttonCount; }
int LayoutComponent::getStripCount() const { return stripCount; }
int LayoutComponent::getKeyRowCount() const { return keyRowCount; }
const int* LayoutComponent::getKeyRowLengths() const { return keyRowLengths; }
int LayoutComponent::getTotalKeyCount() const { return normalKeyCount + percKeyCount + buttonCount; }
int LayoutComponent::getPercKeyStartIndex() const { return normalKeyCount; }
int LayoutComponent::getButtonStartIndex() const { return normalKeyCount + percKeyCount; }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
void LayoutComponent::setKeyCounts(InstrumentType deviceType) {
    switch(deviceType) {
        case InstrumentType::None:
            normalKeyCount = 0; percKeyCount = 0; keyRowCount = 0;
            for (int& keyRowLength : keyRowLengths) keyRowLength = 0;
            buttonCount = 0; stripCount = 0;
            break;
        case InstrumentType::Alpha:
            normalKeyCount = 120; percKeyCount = 12; keyRowCount = 5;
            for (int i=0; i<5; ++i) keyRowLengths[i] = 24;
            buttonCount = 0; stripCount = 2;
            break;
        case InstrumentType::Tau:
            normalKeyCount = 72; percKeyCount = 12; keyRowCount = 4;
            keyRowLengths[0] = 16; keyRowLengths[1] = 16; keyRowLengths[2] = 20; keyRowLengths[3] = 20; keyRowLengths[4] = 0;
            buttonCount = 8; stripCount = 1;
            break;
        case InstrumentType::Pico:
            normalKeyCount = 18; percKeyCount = 0; keyRowCount = 2;
            keyRowLengths[0] = 9; keyRowLengths[1] = 9; keyRowLengths[2] = 0; keyRowLengths[3] = 0; keyRowLengths[4] = 0;
            buttonCount = 4; stripCount = 1;
            break;
        default: break;
    }
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

} // namespace ecm
