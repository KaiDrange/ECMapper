#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "Utility.h"
#include "DropdownComponent.h"
#include "NumberInputComponent.h"
#include "../Models/ZoneWrapper.h"

class ZonePanelComponent  : public PanelComponent, public juce::ValueTree::Listener {
public:
    ZonePanelComponent(DeviceType deviceType, Zone zone, float widthFactor, float heightFactor, juce::AudioProcessorValueTreeState &pluginState);
    ~ZonePanelComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void setStandardMidiDropdownParams(DropdownComponent &dropdown, juce::Identifier treeId, const ZoneWrapper::MidiValue &defaultValue);

    juce::Label label;
    juce::ToggleButton enableZoneButton;

    DropdownComponent midiChannelDropdown;
    DropdownComponent pressureDropdown;
    DropdownComponent yawDropdown;
    DropdownComponent rollDropdown;
    DropdownComponent strip1RelativeDropdown;
    DropdownComponent strip1AbsoluteDropdown;
    DropdownComponent strip2RelativeDropdown;
    DropdownComponent strip2AbsoluteDropdown;
    DropdownComponent breathDropdown;
    NumberInputComponent transposeInput;
    NumberInputComponent keyPitchbendRangeInput;
    NumberInputComponent channelMaxPBInput;
    
    DeviceType deviceType;
    Zone zone;
    
    juce::AudioProcessorValueTreeState &pluginState;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZonePanelComponent)
};
