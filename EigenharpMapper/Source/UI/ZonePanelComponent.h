#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"
#include "Utility.h"
#include "DropdownComponent.h"
#include "NumberInputComponent.h"

class ZonePanelComponent  : public PanelComponent {
public:
    ZonePanelComponent(int zoneNumber, float widthFactor, float heightFactor);
    ~ZonePanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setStandardMidiDropdownParams(DropdownComponent &dropdown, int defaultId);
    int zoneNumber;
    bool enabled;
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
    NumberInputComponent globalPitchbendRangeInput;
    NumberInputComponent keyPitchbendRangeInput;
    DropdownComponent deviceOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZonePanelComponent)
};
