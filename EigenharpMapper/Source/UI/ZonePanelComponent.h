#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "Utility.h"
#include "DropdownComponent.h"
#include "NumberInputComponent.h"
#include "../Models/ZoneConfig.h"
#include "../Models/ZoneWrapper.h"

class ZonePanelComponent  : public PanelComponent, public juce::ValueTree::Listener {
public:
    ZonePanelComponent(int tabIndex, int zoneNumber, float widthFactor, float heightFactor, juce::ValueTree parentTree);
    ~ZonePanelComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    ZoneConfig* getZoneConfig();

private:
    void setStandardMidiDropdownParams(DropdownComponent &dropdown, juce::Identifier treeId, const ZoneWrapper::MidiValue &defaultValue);
    void updateStandardMidiParamsDropdown(juce::Identifier &id, DropdownComponent &dropdown);

    int zoneNumber;
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
    
    ZoneConfig zoneConfig;
    DeviceType deviceType; // temp until ZoneConfig is gone
    Zone zone; // temp until ZoneConfig is gone
    
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZonePanelComponent)
};
