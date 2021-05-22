#include <JuceHeader.h>
#include "ZonePanelComponent.h"

ZonePanelComponent::ZonePanelComponent(int zoneNumber, float widthFactor, float heightFactor):
        PanelComponent(widthFactor, heightFactor),
        transposeInput("Transpose:", 3, -24, 24, 0),
        globalPitchbendRangeInput("Global pitchbend:", 2, 0, 96, 12),
        keyPitchbendRangeInput("Key pitchbend:", 2, 0, 96, 2),
        zoneConfig((Zone)zoneNumber) {
    this->zoneNumber = zoneNumber;
    addAndMakeVisible(label);
    label.setText("Zone " + juce::String(zoneNumber), juce::NotificationType::dontSendNotification);

    addAndMakeVisible(enableZoneButton);
    enableZoneButton.setButtonText("On");
    enableZoneButton.onClick = [&] {
        zoneConfig.setEnabled(enableZoneButton.getToggleState());
    };
    
    transposeInput.input.onTextChange = [&] {
        zoneConfig.setTranspose(transposeInput.getValue());
    };
            
    addAndMakeVisible(midiChannelDropdown);
    midiChannelDropdown.setLabelText("Midi channel:");
    for (int i = 1; i <= 16; i++)
        midiChannelDropdown.addItem(juce::String(i), i);
    midiChannelDropdown.addItem("MPE1_16", 17);
    midiChannelDropdown.addItem("MPE1_14", 18);
    midiChannelDropdown.addItem("MPE1_12", 19);
    midiChannelDropdown.addItem("MPE1_8", 20);
    midiChannelDropdown.addItem("MPE9_16", 21);
    midiChannelDropdown.addItem("MPE13_16", 22);

    midiChannelDropdown.setSelectedItemId(17);
    midiChannelDropdown.box.onChange = [&] {
        zoneConfig.setMidiChannelType((MidiChannelType)midiChannelDropdown.box.getSelectedId());
    };
            
    
    addAndMakeVisible(pressureDropdown);
    pressureDropdown.setLabelText("Pressure:");
    setStandardMidiDropdownParams(pressureDropdown, 129);
    addAndMakeVisible(yawDropdown);
    yawDropdown.setLabelText("Yaw:");
    setStandardMidiDropdownParams(yawDropdown, 75);
    addAndMakeVisible(rollDropdown);
    rollDropdown.setLabelText("Roll:");
    setStandardMidiDropdownParams(rollDropdown, 131);

    addAndMakeVisible(strip1RelativeDropdown);
    strip1RelativeDropdown.setLabelText("Strip1 Rel:");
    setStandardMidiDropdownParams(strip1RelativeDropdown, 131);
    addAndMakeVisible(strip1AbsoluteDropdown);
    strip1AbsoluteDropdown.setLabelText("Strip1 Abs:");
    setStandardMidiDropdownParams(strip1AbsoluteDropdown, 132);
    addAndMakeVisible(strip2RelativeDropdown);
    strip2RelativeDropdown.setLabelText("Strip2 Rel:");
    setStandardMidiDropdownParams(strip2RelativeDropdown, 132);
    addAndMakeVisible(strip2AbsoluteDropdown);
    strip2AbsoluteDropdown.setLabelText("Strip2 Abs:");
    setStandardMidiDropdownParams(strip2AbsoluteDropdown, 132);
    addAndMakeVisible(breathDropdown);
    breathDropdown.setLabelText("Breath:");
    setStandardMidiDropdownParams(breathDropdown, 3);
    
    addAndMakeVisible(transposeInput);
    addAndMakeVisible(globalPitchbendRangeInput);
    addAndMakeVisible(keyPitchbendRangeInput);
    
//    addAndMakeVisible(deviceOutput);
//    deviceOutput.setLabelText("Output:");
//    deviceOutput.addItem("Midi out 1",1);
//    deviceOutput.setSelectedItemId(1);

    zoneConfig.addListener(this);
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
    auto topArea = area.removeFromTop(lineHeight);
    label.setBounds(topArea.removeFromLeft(area.getWidth()*0.2));
    enableZoneButton.setBounds(topArea.removeFromRight(area.getWidth()*0.1));

    auto col1 = area.removeFromLeft(colWidth);
    col1.reduce(col1.getWidth()*0.01, col1.getWidth()*0.01);
    midiChannelDropdown.setBounds(col1.removeFromTop(lineHeight));
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
    transposeInput.setBounds(col2.removeFromTop(lineHeight));
    globalPitchbendRangeInput.setBounds(col2.removeFromTop(lineHeight));
    keyPitchbendRangeInput.setBounds(col2.removeFromTop(lineHeight));
    
    deviceOutput.setBounds(col2.removeFromBottom(lineHeight));
}

void ZonePanelComponent::setStandardMidiDropdownParams(DropdownComponent &dropdown, int defaultId) {
    for (int i = 0; i < 128; i++)
    dropdown.addItem("CC #" + juce::String(i), i+1);
    dropdown.addItem("Chan aftertouch", 129);
    dropdown.addItem("Poly aftertouch", 130);
    dropdown.addItem("Pitchbend", 131);
    dropdown.addItem("Off", 132);
    dropdown.setSelectedItemId(defaultId);
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
}


