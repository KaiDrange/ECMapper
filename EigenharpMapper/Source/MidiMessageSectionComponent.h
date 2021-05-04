#pragma once
#include <JuceHeader.h>
#include "NumberInputComponent.h"

class MidiMessageSectionComponent  : public juce::Component
{
public:
    MidiMessageSectionComponent();
    ~MidiMessageSectionComponent() override;

    void resized() override;

private:
    juce::GroupComponent cmdKeyTypeRadioGroup;
    juce::ToggleButton cmdKeyTypeLatch;
    juce::ToggleButton cmdKeyTypeMomentary;
    juce::ToggleButton cmdKeyTypeTrigger;

    juce::GroupComponent midiMsgTypeRadioGroup;
    juce::ToggleButton midiMsgTypeCC;
    juce::ToggleButton midiMsgTypeProgChange;
    juce::ToggleButton midiMsgTypeTransport;
    juce::ToggleButton midiMsgTypeAllNotesOff;
    
    NumberInputComponent midiCmdValue;
    NumberInputComponent offValue;
    NumberInputComponent onValue;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMessageSectionComponent)
};
