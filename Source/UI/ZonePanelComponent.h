#pragma once
#include <JuceHeader.h>
#include "PanelComponent.h"
#include "DropdownComponent.h"
#include "NumberInputComponent.h"
#include "../Core/ZoneWrapper.h"
#include "../Core/Utils.h"

namespace ecm {

class ZonePanelComponent  : public PanelComponent, public juce::ValueTree::Listener {
public:
    ZonePanelComponent(InstrumentType deviceType, Zone zone, float widthFactor, float heightFactor, juce::AudioProcessorValueTreeState& pluginState);
    ~ZonePanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void refreshFromState();

private:
    void valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged(juce::ValueTree&) override {}
    void valueTreeRedirected(juce::ValueTree&) override {}

    void setStandardMidiDropdownParams(DropdownComponent& dropdown, juce::Identifier treeId, const ZoneWrapper::MidiValue& defaultValue);

    juce::Label label;
    juce::ToggleButton enableZoneButton { "On" };

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
    
    InstrumentType deviceType;
    Zone zone;
    juce::AudioProcessorValueTreeState& pluginState;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZonePanelComponent)
};

} // namespace ecm
