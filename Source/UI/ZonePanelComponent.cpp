#include "ZonePanelComponent.h"
#include "AppStyle.h"

namespace ecm {

ZonePanelComponent::ZonePanelComponent(InstrumentType deviceType, Zone zone, float widthFactor, float heightFactor, juce::AudioProcessorValueTreeState& pluginState)
    : PanelComponent(widthFactor, heightFactor), 
      transposeInput("Transpose:", 3, -96, 96, false), 
      keyPitchbendRangeInput("Key pitchbend:", 2, 0, 96, false), 
      channelMaxPBInput("Channel max pb:", 2, 0, 96, false), 
      deviceType(deviceType), 
      zone(zone), 
      pluginState(pluginState) {
    
    addAndMakeVisible(label);
    label.setText("Zone " + juce::String(static_cast<int>(zone)), juce::dontSendNotification);

    addAndMakeVisible(enableZoneButton);
    if (auto* param = dynamic_cast<juce::AudioParameterBool*>(
            pluginState.getParameter(ZoneWrapper::getEnabledParameterID(deviceType, zone)))) {
        enableZoneButton.setToggleState(param->get(), juce::dontSendNotification);
    } else {
        enableZoneButton.setToggleState(ZoneWrapper::getEnabled(deviceType, zone, pluginState.state), juce::dontSendNotification);
    }
    enableZoneButton.onClick = [this] {
        auto enabled = enableZoneButton.getToggleState();
        ZoneWrapper::setEnabled(this->deviceType, this->zone, enabled, this->pluginState.state);
        if (auto* param = dynamic_cast<juce::AudioParameterBool*>(
                this->pluginState.getParameter(ZoneWrapper::getEnabledParameterID(this->deviceType, this->zone)))) {
            param->setValueNotifyingHost(enabled);
        }
    };
    ZoneWrapper::addListener(deviceType, this, pluginState.state);
    
    addAndMakeVisible(transposeInput);
    if (auto* param = dynamic_cast<juce::AudioParameterInt*>(
            pluginState.getParameter(ZoneWrapper::getTransposeParameterID(deviceType, zone)))) {
        transposeInput.setValue(param->get());
    } else {
        transposeInput.setValue(ZoneWrapper::getTranspose(deviceType, zone, pluginState.state));
    }
    transposeInput.input.onFocusLost = [this] {
        auto value = transposeInput.getValue();
        ZoneWrapper::setTranspose(this->deviceType, this->zone, value, this->pluginState.state);
        if (auto* param = dynamic_cast<juce::AudioParameterInt*>(
                this->pluginState.getParameter(ZoneWrapper::getTransposeParameterID(this->deviceType, this->zone)))) {
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float) value));
        }
    };

    addAndMakeVisible(keyPitchbendRangeInput);
    keyPitchbendRangeInput.setValue(ZoneWrapper::getKeyPitchbend(deviceType, zone, pluginState.state));
    keyPitchbendRangeInput.input.onTextChange = [this] {
        ZoneWrapper::setKeyPitchbend(this->deviceType, this->zone, keyPitchbendRangeInput.getValue(), this->pluginState.state);
    };
    
    addAndMakeVisible(channelMaxPBInput);
    channelMaxPBInput.setValue(ZoneWrapper::getChannelMaxPitchbend(deviceType, zone, pluginState.state));
    channelMaxPBInput.input.onTextChange = [this] {
        ZoneWrapper::setChannelMaxPitchbend(this->deviceType, this->zone, channelMaxPBInput.getValue(), this->pluginState.state);
    };

    addAndMakeVisible(midiChannelDropdown);
    midiChannelDropdown.setLabelText("Midi channel:", false);
    for (int i = 1; i <= 16; i++) midiChannelDropdown.addItem(juce::String(i), i);
    midiChannelDropdown.addItem("MPE Lower", 17);
    midiChannelDropdown.addItem("MPE Upper", 18);

    midiChannelDropdown.setSelectedItemId(static_cast<int>(ZoneWrapper::getMidiChannelType(deviceType, zone, pluginState.state)));
    midiChannelDropdown.box.onChange = [this] {
        ZoneWrapper::setMidiChannelType(this->deviceType, this->zone, static_cast<MidiChannelType>(midiChannelDropdown.box.getSelectedId()), this->pluginState.state);
    };
    
    pressureDropdown.setLabelText("Pressure:", false);
    setStandardMidiDropdownParams(pressureDropdown, ZoneWrapper::id_pressure, ZoneWrapper::default_pressure);
            
    yawDropdown.setLabelText("Yaw:", false);
    setStandardMidiDropdownParams(yawDropdown, ZoneWrapper::id_yaw, ZoneWrapper::default_yaw);

    rollDropdown.setLabelText("Roll:", false);
    setStandardMidiDropdownParams(rollDropdown, ZoneWrapper::id_roll, ZoneWrapper::default_roll);

    strip1RelativeDropdown.setLabelText("Strip1 Rel:", false);
    setStandardMidiDropdownParams(strip1RelativeDropdown, ZoneWrapper::id_strip1Rel, ZoneWrapper::default_strip1Rel);

    strip1AbsoluteDropdown.setLabelText("Strip1 Abs:", false);
    setStandardMidiDropdownParams(strip1AbsoluteDropdown, ZoneWrapper::id_strip1Abs, ZoneWrapper::default_strip1Abs);

    strip2RelativeDropdown.setLabelText("Strip2 Rel:", false);
    setStandardMidiDropdownParams(strip2RelativeDropdown, ZoneWrapper::id_strip2Rel, ZoneWrapper::default_strip2Rel);

    strip2AbsoluteDropdown.setLabelText("Strip2 Abs:", false);
    setStandardMidiDropdownParams(strip2AbsoluteDropdown, ZoneWrapper::id_strip2Abs, ZoneWrapper::default_strip2Abs);

    breathDropdown.setLabelText("Breath:", false);
    setStandardMidiDropdownParams(breathDropdown, ZoneWrapper::id_breath, ZoneWrapper::default_breath);
}

ZonePanelComponent::~ZonePanelComponent() {
    ZoneWrapper::removeListener(deviceType, this, pluginState.state);
}

void ZonePanelComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Style::surface());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(Style::zoneColour(zone));
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
}

void ZonePanelComponent::resized() {
    auto area = getLocalBounds();
    area.reduce(area.getWidth() * 0.01f, area.getWidth() * 0.01f);
    float lineHeight = area.getHeight() * 0.1f;
    auto colWidth = area.getWidth() / 2;
    
    auto topArea = area.removeFromTop(lineHeight * 1.5f);
    label.setBounds(topArea.removeFromLeft(area.getWidth() * 0.2f));
    enableZoneButton.setBounds(topArea.removeFromRight(area.getWidth() * 0.1f));

    auto col1 = area.removeFromLeft(colWidth);
    col1.reduce(col1.getWidth() * 0.01f, col1.getWidth() * 0.01f);
    pressureDropdown.setBounds(col1.removeFromTop(lineHeight));
    yawDropdown.setBounds(col1.removeFromTop(lineHeight));
    rollDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip1RelativeDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip1AbsoluteDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip2RelativeDropdown.setBounds(col1.removeFromTop(lineHeight));
    strip2AbsoluteDropdown.setBounds(col1.removeFromTop(lineHeight));
    breathDropdown.setBounds(col1.removeFromTop(lineHeight));
    
    auto col2 = area;
    col2.reduce(col2.getWidth() * 0.01f, col2.getWidth() * 0.01f);
    midiChannelDropdown.setBounds(col2.removeFromTop(lineHeight));
    transposeInput.setBounds(col2.removeFromTop(lineHeight));
    keyPitchbendRangeInput.setBounds(col2.removeFromTop(lineHeight));
    channelMaxPBInput.setBounds(col2.removeFromTop(lineHeight));
}

void ZonePanelComponent::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    if (property != ZoneWrapper::id_enabled)
        return;

    if (ZoneWrapper::getInstrumentTypeFromTree(vTree) != deviceType)
        return;

    auto typeStr = vTree.getType().toString();
    if (!typeStr.startsWith(ZoneWrapper::id_zone.toString()))
        return;

    auto changedZone = static_cast<Zone>(typeStr.substring(4).getIntValue());
    if (changedZone != zone)
        return;

    enableZoneButton.setToggleState(ZoneWrapper::getEnabled(deviceType, zone, pluginState.state), juce::dontSendNotification);
}

void ZonePanelComponent::refreshFromState()
{
    enableZoneButton.setToggleState(ZoneWrapper::getEnabled(deviceType, zone, pluginState.state), juce::dontSendNotification);
    transposeInput.setValue(ZoneWrapper::getTranspose(deviceType, zone, pluginState.state));
    keyPitchbendRangeInput.setValue(ZoneWrapper::getKeyPitchbend(deviceType, zone, pluginState.state));
    channelMaxPBInput.setValue(ZoneWrapper::getChannelMaxPitchbend(deviceType, zone, pluginState.state));
    midiChannelDropdown.setSelectedItemId(static_cast<int>(ZoneWrapper::getMidiChannelType(deviceType, zone, pluginState.state)));

    auto setMidiDropdown = [this](DropdownComponent& dropdown, juce::Identifier treeId, const ZoneWrapper::MidiValue& defaultValue)
    {
        auto midiValue = ZoneWrapper::getMidiValue(deviceType, zone, treeId, defaultValue, pluginState.state);
        if (midiValue.valueType == MidiValueType::CC)
            dropdown.box.setSelectedItemIndex(midiValue.ccNo);
        else
            dropdown.box.setSelectedItemIndex(126 + static_cast<int>(midiValue.valueType));
    };

    setMidiDropdown(pressureDropdown, ZoneWrapper::id_pressure, ZoneWrapper::default_pressure);
    setMidiDropdown(yawDropdown, ZoneWrapper::id_yaw, ZoneWrapper::default_yaw);
    setMidiDropdown(rollDropdown, ZoneWrapper::id_roll, ZoneWrapper::default_roll);
    setMidiDropdown(strip1RelativeDropdown, ZoneWrapper::id_strip1Rel, ZoneWrapper::default_strip1Rel);
    setMidiDropdown(strip1AbsoluteDropdown, ZoneWrapper::id_strip1Abs, ZoneWrapper::default_strip1Abs);
    setMidiDropdown(strip2RelativeDropdown, ZoneWrapper::id_strip2Rel, ZoneWrapper::default_strip2Rel);
    setMidiDropdown(strip2AbsoluteDropdown, ZoneWrapper::id_strip2Abs, ZoneWrapper::default_strip2Abs);
    setMidiDropdown(breathDropdown, ZoneWrapper::id_breath, ZoneWrapper::default_breath);
}

void ZonePanelComponent::setStandardMidiDropdownParams(DropdownComponent& dropdown, juce::Identifier treeId, const ZoneWrapper::MidiValue& defaultValue) {
    for (int i = 0; i < 128; i++) dropdown.addItem("CC #" + juce::String(i), i + 1);
    dropdown.addItem("Pitchbend", 129);
    dropdown.addItem("Chan aftertouch", 130);
    dropdown.addItem("Poly aftertouch", 131);
    dropdown.addItem("Off", 132);

    auto midiValue = ZoneWrapper::getMidiValue(deviceType, zone, treeId, defaultValue, pluginState.state);
    if (midiValue.valueType == MidiValueType::CC)
        dropdown.box.setSelectedItemIndex(midiValue.ccNo);
    else
        dropdown.box.setSelectedItemIndex(126 + static_cast<int>(midiValue.valueType));

    dropdown.box.onChange = [this, &dropdown, treeId] {
        ZoneWrapper::MidiValue mv;
        auto selIndex = dropdown.box.getSelectedItemIndex();
        if (selIndex < 128) {
            mv.valueType = MidiValueType::CC;
            mv.ccNo = selIndex;
        } else {
            mv.valueType = static_cast<MidiValueType>(selIndex - 126);
            mv.ccNo = 0;
        }
        ZoneWrapper::setMidiValue(this->deviceType, this->zone, treeId, mv, this->pluginState.state);
    };
    
    addAndMakeVisible(dropdown);
}

} // namespace ecm
