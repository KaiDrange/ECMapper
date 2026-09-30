#include "LayoutComponent.h"

#include "AppStyle.h"

namespace ecm {

namespace {
double pitchBendPercent(int value) {
    const int offset = value - 8192;
    return 100.0 * offset / (offset < 0 ? 8192.0 : 8191.0);
}

int pitchBendValue(double percent) {
    return juce::jlimit(0, 16383, juce::roundToInt(8192.0 + percent * (percent < 0.0 ? 8192.0 : 8191.0) / 100.0));
}
}


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
        if (LayoutWrapper::supportsJoystick(activeKeyId)) {
            menu.addItem("Joystick", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Joystick, this->pluginState.state); showHidePanels(); repaint(); });
            menu.addItem("Touche", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Touche, this->pluginState.state); showHidePanels(); repaint(); });
        }
        menu.addItem("Chord", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::Chord, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("Midi msg", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::MidiMsg, this->pluginState.state); showHidePanels(); repaint(); });
        menu.addItem("App Ctrl", [this] { LayoutWrapper::setKeyMappingType(activeKeyId, KeyMappingType::AppCtrl, this->pluginState.state); showHidePanels(); repaint(); });
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(mapTypeMenuButton));
    };

    const std::array<juce::String, 5> joystickLabels { "Roll -", "Roll +", "Yaw -", "Yaw +", "Press" };
    for (size_t i = 0; i < joystickRows.size(); ++i) {
        auto& row = joystickRows[i];
        addChildComponent(row.label);
        row.label.setText(joystickLabels[i], juce::dontSendNotification);
        addChildComponent(row.type);
        row.type.addItem("Off", static_cast<int>(MidiValueType::Off));
        row.type.addItem("CC", static_cast<int>(MidiValueType::CC));
        row.type.addItem("Pitch bend", static_cast<int>(MidiValueType::Pitchbend));
        addChildComponent(row.ccLabel);
        row.ccLabel.setText("CC#", juce::dontSendNotification);
        for (auto* slider : { &row.number, &row.minimum, &row.maximum }) {
            addChildComponent(*slider);
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setScrollWheelEnabled(false);
            slider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 75, 22);
            slider->setRange(0, 127, 1);
            slider->onValueChange = [this] { updateJoystickSettings(); };
        }
        addChildComponent(row.rangeDash);
        row.rangeDash.setText("-", juce::dontSendNotification);
        row.rangeDash.setJustificationType(juce::Justification::centred);
        row.number.setTooltip("CC controller number (0-127)");
        row.minimum.setTooltip("Minimum: sent at center and on release. Pitch bend: -100% to 100%, with 0% at center.");
        row.maximum.setTooltip("Maximum: sent at full movement or pressure. Reversed ranges are supported.");
        row.type.onChange = [this, i] {
            auto& r = joystickRows[i];
            const bool pitchBend = r.type.getSelectedId() == static_cast<int>(MidiValueType::Pitchbend);
            {
                const juce::ScopedValueSetter<bool> loading(loadingJoystick, true);
                for (auto* input : { &r.minimum, &r.maximum }) {
                    input->setRange(pitchBend ? -100 : 0, pitchBend ? 100 : 127, pitchBend ? 0.1 : 1.0);
                    input->setTextValueSuffix(pitchBend ? "%" : "");
                }
                r.minimum.setValue(0, juce::dontSendNotification);
                r.maximum.setValue(pitchBend ? 100 : 127, juce::dontSendNotification);
            }
            updateJoystickSettings();
            showHidePanels();
        };
    }
    addChildComponent(joystickChannel);
    joystickChannel.addItem("Channel: Zone / MPE master", 1);
    for (int channel = 1; channel <= 16; ++channel)
        joystickChannel.addItem("Channel: " + juce::String(channel), channel + 1);
    joystickChannel.onChange = [this] { updateJoystickSettings(); };

    addChildComponent(palmMuteModeSelector);
    palmMuteModeSelector.addItem("Mode: Latch", 1);
    palmMuteModeSelector.addItem("Mode: Momentary", 2);
    palmMuteModeSelector.setTooltip("Latch toggles palm mute on each press. Momentary mutes while held. Momentary pressure varies the mute from 80 ms (soft) to 10 ms (firm), with higher note-off velocity for firmer pressure. Latch always uses 10 ms and sends no pressure CC. Affects this device's selected zone.");
    palmMuteModeSelector.onChange = [this] {
        if (activeKeyId.deviceType == InstrumentType::None) return;
        LayoutWrapper::setKeyMappingValue(activeKeyId, palmMuteModeSelector.getSelectedId() == 2
            ? "PalmMute;Momentary" : "PalmMute;Latch", this->pluginState.state);
        showHidePanels();
    };

    addChildComponent(palmMutePressureCCSelector);
    palmMutePressureCCSelector.addItem("Pressure CC: Off", 1);
    for (int cc = 0; cc < 128; ++cc)
        palmMutePressureCCSelector.addItem("Pressure CC: " + juce::String(cc), cc + 2);
    palmMutePressureCCSelector.setTooltip("Send key pressure on this zone's channel (MPE master channel). Returns to zero on release.");
    palmMutePressureCCSelector.onChange = [this] {
        LayoutWrapper::setPalmMutePressureCC(activeKeyId, palmMutePressureCCSelector.getSelectedId() - 2, this->pluginState.state);
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
    
    if (joystickChannel.isVisible()) {
        const int height = juce::jmax(22, static_cast<int>(areaHeight * 0.04f));
        menuArea.removeFromTop(10);
        joystickChannel.setBounds(menuArea.removeFromTop(height));
        menuArea.removeFromTop(10);
        for (size_t i = 0; i < joystickRows.size(); ++i) {
            auto& row = joystickRows[i];
            if (!row.type.isVisible()) continue;
            // Separate roll, yaw and pressure while keeping each direction pair together.
            if (i == 2 || i == 4)
                menuArea.removeFromTop(10);
            auto heading = menuArea.removeFromTop(height);
            row.label.setBounds(heading.removeFromLeft(heading.getWidth() / 3));
            row.type.setBounds(heading);
            auto values = menuArea.removeFromTop(height);
            if (row.number.isVisible()) {
                row.ccLabel.setBounds(values.removeFromLeft(values.getWidth() / 3));
                row.number.setBounds(values);
                values = menuArea.removeFromTop(height);
            }
            row.minimum.setBounds(values.removeFromLeft((values.getWidth() - 20) / 2));
            row.rangeDash.setBounds(values.removeFromLeft(20));
            row.maximum.setBounds(values);
            menuArea.removeFromTop(6);
        }
    }
    if (palmMuteModeSelector.isVisible()) {
        menuArea.removeFromTop(8);
        palmMuteModeSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
        menuArea.removeFromTop(8);
        palmMutePressureCCSelector.setBounds(menuArea.removeFromTop(static_cast<int>(areaHeight * 0.04f)));
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
    joystickChannel.setEnabled(enable);
    for (auto& row : joystickRows) {
        row.type.setEnabled(enable);
        row.number.setEnabled(enable);
        row.minimum.setEnabled(enable);
        row.maximum.setEnabled(enable);
    }
    colourMenuButton.setEnabled(enable);
    zoneMenuButton.setEnabled(enable);
    mapTypeMenuButton.setEnabled(enable);
    stringSelector.setEnabled(enable);
    palmMuteModeSelector.setEnabled(enable);
    palmMutePressureCCSelector.setEnabled(enable);
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
    const juce::ScopedValueSetter<bool> loading(loadingJoystick, true);
    const bool touche = layoutKey.keyMappingType == KeyMappingType::Touche;
    const bool joystick = touche || layoutKey.keyMappingType == KeyMappingType::Joystick;
    joystickChannel.setVisible(joystick);
    const auto joystickSettings = LayoutWrapper::getJoystickSettings(activeKeyId, pluginState.state);
    joystickChannel.setSelectedId(joystickSettings.midiChannel + 1, juce::dontSendNotification);
    for (size_t i = 0; i < joystickRows.size(); ++i) {
        auto& row = joystickRows[i];
        const auto& assignment = joystickSettings.assignments[i];
        const bool visible = joystick && (!touche || i < 4);
        const std::array<juce::String, 5> labels { touche ? "Yaw -" : "Roll -", touche ? "Yaw +" : "Roll +", touche ? "Press 1" : "Yaw -", touche ? "Press 2" : "Yaw +", "Press" };
        row.label.setText(labels[i], juce::dontSendNotification);
        row.label.setTooltip(touche && i >= 2 ? (i == 2
            ? "Pressure at negative roll; full pressure at center."
            : "Pressure at positive roll; full pressure at center.") : "");
        row.label.setVisible(visible);
        row.type.setVisible(visible);
        row.number.setVisible(visible && assignment.type == MidiValueType::CC);
        row.ccLabel.setVisible(visible && assignment.type == MidiValueType::CC);
        row.rangeDash.setVisible(visible);
        row.minimum.setVisible(visible);
        row.maximum.setVisible(visible);
        row.type.setSelectedId(static_cast<int>(assignment.type), juce::dontSendNotification);
        const bool pitchBend = assignment.type == MidiValueType::Pitchbend;
        for (auto* input : { &row.minimum, &row.maximum }) {
            input->setRange(pitchBend ? -100 : 0, pitchBend ? 100 : 127, pitchBend ? 0.1 : 1.0);
            input->setTextValueSuffix(pitchBend ? "%" : "");
        }
        row.number.setValue(assignment.number, juce::dontSendNotification);
        row.minimum.setValue(pitchBend ? pitchBendPercent(assignment.minimum) : assignment.minimum, juce::dontSendNotification);
        row.maximum.setValue(pitchBend ? pitchBendPercent(assignment.maximum) : assignment.maximum, juce::dontSendNotification);
        row.number.setEnabled(mapTypeMenuButton.isEnabled() && assignment.type == MidiValueType::CC);
    }
    const bool strum = layoutKey.keyMappingType == KeyMappingType::Strum;
    palmMuteModeSelector.setVisible(layoutKey.keyMappingType == KeyMappingType::PalmMute);
    palmMutePressureCCSelector.setVisible(layoutKey.keyMappingType == KeyMappingType::PalmMute
        && layoutKey.mappingValue == "PalmMute;Momentary");
    palmMutePressureCCSelector.setSelectedId(LayoutWrapper::getPalmMutePressureCC(activeKeyId, pluginState.state) + 2, juce::dontSendNotification);
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
    joystickChannel.setVisible(false);
    for (auto& row : joystickRows) {
        row.label.setVisible(false);
        row.ccLabel.setVisible(false);
        row.type.setVisible(false);
        row.number.setVisible(false);
        row.rangeDash.setVisible(false);
        row.minimum.setVisible(false);
        row.maximum.setVisible(false);
    }
    stringSelector.setVisible(false);
    palmMuteModeSelector.setVisible(false);
    palmMutePressureCCSelector.setVisible(false);
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

void LayoutComponent::updateJoystickSettings() {
    if (loadingJoystick) return;
    if (activeKeyId.deviceType == InstrumentType::None) return;
    LayoutWrapper::JoystickSettings settings;
    settings.midiChannel = joystickChannel.getSelectedId() - 1;
    for (size_t i = 0; i < joystickRows.size(); ++i) {
        const auto& row = joystickRows[i];
        const auto type = static_cast<MidiValueType>(row.type.getSelectedId());
        const bool pitchBend = type == MidiValueType::Pitchbend;
        settings.assignments[i] = { type, static_cast<int>(row.number.getValue()),
            pitchBend ? pitchBendValue(row.minimum.getValue()) : static_cast<int>(row.minimum.getValue()),
            pitchBend ? pitchBendValue(row.maximum.getValue()) : static_cast<int>(row.maximum.getValue()) };
    }
    LayoutWrapper::setJoystickSettings(activeKeyId, settings, pluginState.state);
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
