#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "Utility.h"
#include "DropdownComponent.h"
#include "NumberInputComponent.h"
#include "../Models/ZoneConfig.h"

class ZonePanelComponent  : public PanelComponent, public juce::ValueTree::Listener {
public:
    ZonePanelComponent(int tabIndex, int zoneNumber, float widthFactor, float heightFactor, juce::ValueTree parentTree);
    ~ZonePanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    ZoneConfig* getZoneConfig();

private:
    void setStandardMidiDropdownParams(DropdownComponent &dropdown, int defaultId, juce::Identifier treeId);
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
//    NumberInputComponent globalPitchbendRangeInput;
    NumberInputComponent keyPitchbendRangeInput;
//    DropdownComponent deviceOutput;
    
    ZoneConfig zoneConfig;
    
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZonePanelComponent)
};
