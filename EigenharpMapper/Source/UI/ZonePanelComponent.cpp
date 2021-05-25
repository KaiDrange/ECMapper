#include <JuceHeader.h>
#include "ZonePanelComponent.h"

ZonePanelComponent::ZonePanelComponent(int tabIndex, int zoneNumber, float widthFactor, float heightFactor, juce::ValueTree parentTree): PanelComponent(widthFactor, heightFactor), transposeInput("Transpose:", 3, -24, 24, 0, false), keyPitchbendRangeInput("Key pitchbend:", 2, 0, 96, 2, false), zoneConfig(tabIndex, (Zone)zoneNumber, parentTree) {
    this->zoneNumber = zoneNumber;
    this->deviceType = (DeviceType)(tabIndex+1);
    this->zone = (Zone)zoneNumber;
    addAndMakeVisible(label);
    label.setText("Zone " + juce::String(zoneNumber), juce::NotificationType::dontSendNotification);

    addAndMakeVisible(enableZoneButton);
    enableZoneButton.setButtonText("On");
    enableZoneButton.setToggleState(ZoneWrapper::getEnabled(deviceType, zone), juce::dontSendNotification);
    enableZoneButton.onClick = [&] {
        ZoneWrapper::setEnabled(deviceType, zone, enableZoneButton.getToggleState());
    };
    
    addAndMakeVisible(transposeInput);
    transposeInput.setValue(ZoneWrapper::getTranspose(deviceType, zone));
    transposeInput.input.onFocusLost = [&] {
        ZoneWrapper::setTranspose(deviceType, zone, transposeInput.getValue());
    };

    addAndMakeVisible(keyPitchbendRangeInput);
    keyPitchbendRangeInput.setValue(ZoneWrapper::getKeyPitchbend(deviceType, zone));
    keyPitchbendRangeInput.input.onTextChange = [&] {
        ZoneWrapper::setKeyPitchbend(deviceType, zone, keyPitchbendRangeInput.getValue());
    };

    addAndMakeVisible(midiChannelDropdown);
    midiChannelDropdown.setLabelText("Midi channel:");
    for (int i = 1; i <= 16; i++)
        midiChannelDropdown.addItem(juce::String(i), i);
    midiChannelDropdown.addItem("MPE Low", 17);
    midiChannelDropdown.addItem("MPE High", 18);

    midiChannelDropdown.setSelectedItemId((int)ZoneWrapper::getMidiChannelType(deviceType, zone));
    midiChannelDropdown.box.onChange = [&] {
        ZoneWrapper::setMidiChannelType(deviceType, zone, (MidiChannelType)midiChannelDropdown.box.getSelectedId());
    };
    
    pressureDropdown.setLabelText("Pressure:");
    setStandardMidiDropdownParams(pressureDropdown, ZoneWrapper::id_pressure, ZoneWrapper::default_pressure);
            
    yawDropdown.setLabelText("Yaw:");
    setStandardMidiDropdownParams(yawDropdown, ZoneWrapper::id_yaw, ZoneWrapper::default_yaw);

    rollDropdown.setLabelText("Roll:");
    setStandardMidiDropdownParams(rollDropdown, ZoneWrapper::id_roll, ZoneWrapper::default_roll);

    strip1RelativeDropdown.setLabelText("Strip1 Rel:");
    setStandardMidiDropdownParams(strip1RelativeDropdown, ZoneWrapper::id_strip1Rel, ZoneWrapper::default_strip1Rel);

    strip1AbsoluteDropdown.setLabelText("Strip1 Abs:");
    setStandardMidiDropdownParams(strip1AbsoluteDropdown, ZoneWrapper::id_strip1Abs, ZoneWrapper::default_strip1Abs);

    strip2RelativeDropdown.setLabelText("Strip2 Rel:");
    setStandardMidiDropdownParams(strip2RelativeDropdown, ZoneWrapper::id_strip2Rel, ZoneWrapper::default_strip2Rel);

    strip2AbsoluteDropdown.setLabelText("Strip2 Abs:");
    setStandardMidiDropdownParams(strip2AbsoluteDropdown, ZoneWrapper::id_strip2Abs, ZoneWrapper::default_strip2Abs);

    breathDropdown.setLabelText("Breath:");
    setStandardMidiDropdownParams(breathDropdown, ZoneWrapper::id_breath, ZoneWrapper::default_breath);

//    zoneConfig.addListener(this);
}

ZonePanelComponent::~ZonePanelComponent() {
}

void ZonePanelComponent::paint(juce::Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour(Utility::zoneEnumToColour((Zone)zoneNumber));
    g.drawRect(getLocalBounds(), 1);
}

void ZonePanelComponent::resized() {
    auto area = getLocalBounds();
    area.reduce(area.getWidth()*0.01, area.getWidth()*0.01);
    auto lineHeight = area.getHeight()*0.1;
    auto colWidth = area.getWidth()*0.5;
    auto topArea = area.removeFromTop(lineHeight*1.5);
    label.setBounds(topArea.removeFromLeft(area.getWidth()*0.2));
    enableZoneButton.setBounds(topArea.removeFromRight(area.getWidth()*0.1));

    auto col1 = area.removeFromLeft(colWidth);
    col1.reduce(col1.getWidth()*0.01, col1.getWidth()*0.01);
    pressureDropdown.setBounds(col1.removeFromTop(lineHeight));
    yawDropdown.setBounds(col1.removeFromTop(lineHeight));
    rollDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip1RelativeDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip1AbsoluteDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip2RelativeDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip2AbsoluteDropdown.setBounds(col1.removeFromTop(lineHeight));
    breathDropdown.setBounds(col1.removeFromTop(lineHeight));
    
    auto col2 = area;
    col2.reduce(col2.getWidth()*0.01, col2.getWidth()*0.01);
    midiChannelDropdown.setBounds(col2.removeFromTop(lineHeight));
    transposeInput.setBounds(col2.removeFromTop(lineHeight));
    keyPitchbendRangeInput.setBounds(col2.removeFromTop(lineHeight));
}

void ZonePanelComponent::setStandardMidiDropdownParams(DropdownComponent &dropdown, juce::Identifier treeId, const ZoneWrapper::MidiValue &defaultValue) {
    for (int i = 0; i < 128; i++)
    dropdown.addItem("CC #" + juce::String(i), i+1);
    dropdown.addItem("Pitchbend", 129);
    dropdown.addItem("Chan aftertouch", 130);
    dropdown.addItem("Poly aftertouch", 131);
    dropdown.addItem("Off", 132);

    ZoneWrapper::MidiValue midiValue = ZoneWrapper::getMidiValue(deviceType, zone, treeId, defaultValue);
    if (midiValue.valueType == MidiValueType::CC)
        dropdown.box.setSelectedItemIndex(midiValue.ccNo);
    else
        dropdown.box.setSelectedItemIndex(126 + (int)midiValue.valueType);

    dropdown.box.onChange = [&, treeId] {
        ZoneWrapper::MidiValue midiValue;
        auto selIndex = dropdown.box.getSelectedItemIndex();
        if (selIndex < 128) {
            midiValue.valueType = MidiValueType::CC;
            midiValue.ccNo = selIndex;
        }
        else {
            midiValue.valueType = (MidiValueType)(selIndex - 126);
            midiValue.ccNo = 0;
        }
        
        ZoneWrapper::setMidiValue(deviceType, zone, treeId, midiValue);
    };
    
    addAndMakeVisible(dropdown);
}

ZoneConfig* ZonePanelComponent::getZoneConfig() {
    return &zoneConfig;
}

void ZonePanelComponent::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    if (property == zoneConfig.id_enabled) {
        enableZoneButton.setToggleState(zoneConfig.isEnabled(), juce::dontSendNotification);
    }
    else if (property == zoneConfig.id_transpose) {
        transposeInput.setValue(zoneConfig.getTranspose());
    }
    else if (property == zoneConfig.id_midiChannelType) {
        midiChannelDropdown.box.setSelectedId((int)zoneConfig.getMidiChannelType());
    }
    else if (property == zoneConfig.id_keyPitchbend) {
        keyPitchbendRangeInput.setValue(zoneConfig.getKeyPitchbend());
    }
    else if (vTree.getType() == zoneConfig.id_pressure) {
        updateStandardMidiParamsDropdown(zoneConfig.id_pressure, pressureDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_yaw) {
        updateStandardMidiParamsDropdown(zoneConfig.id_yaw, yawDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_roll) {
        updateStandardMidiParamsDropdown(zoneConfig.id_roll, rollDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_strip1Rel) {
        updateStandardMidiParamsDropdown(zoneConfig.id_strip1Rel, strip1RelativeDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_strip1Abs) {
        updateStandardMidiParamsDropdown(zoneConfig.id_strip1Abs, strip1AbsoluteDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_strip2Rel) {
        updateStandardMidiParamsDropdown(zoneConfig.id_strip2Rel, strip2RelativeDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_strip2Abs) {
        updateStandardMidiParamsDropdown(zoneConfig.id_strip2Abs, strip2AbsoluteDropdown);
    }
    else if (vTree.getType() == zoneConfig.id_breath) {
        updateStandardMidiParamsDropdown(zoneConfig.id_breath, breathDropdown);
    }
}

void ZonePanelComponent::updateStandardMidiParamsDropdown(juce::Identifier &id, DropdownComponent &dropdown) {
    auto midiValue = zoneConfig.getMidiValue(id);
    if (midiValue.valueType == MidiValueType::CC) {
        if (dropdown.box.getSelectedItemIndex() != midiValue.ccNo)
            dropdown.box.setSelectedItemIndex(midiValue.ccNo);
    }
    else {
        if (dropdown.box.getSelectedItemIndex() != 126 + (int)midiValue.valueType)
            dropdown.box.setSelectedItemIndex(126 + (int)midiValue.valueType);
    }
}
