#pragma once
#include <JuceHeader.h>
#include "NumberInputComponent.h"
#include "DropdownComponent.h"

class MidiMessageSectionComponent  : public juce::Component, NumberInputComponent::Listener
{
public:
    MidiMessageSectionComponent();
    ~MidiMessageSectionComponent() override;

    void resized() override;
    juce::String getMessageString();
    void updatePanelFromMessageString(juce::String msgString);

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void valuesChanged(MidiMessageSectionComponent*) = 0;
    };
    void addListener(Listener *listenerToAdd);
    void removeListener(Listener *listenerToRemove);

private:
    void sendChangeMessage();
    void numberInputChanged(NumberInputComponent*) override;
    void resetPanel();
    juce::GroupComponent cmdKeyTypeRadioGroup;
    juce::ToggleButton cmdKeyTypeLatch;
    juce::ToggleButton cmdKeyTypeMomentary;
    juce::ToggleButton cmdKeyTypeTrigger;

    juce::GroupComponent midiMsgTypeRadioGroup;
    juce::ToggleButton midiMsgTypeCC;
    juce::ToggleButton midiMsgTypeProgChange;
    juce::ToggleButton midiMsgTypeRealtime;
    juce::ToggleButton midiMsgTypeAllNotesOff;
    
    NumberInputComponent midiCmdValue;
    NumberInputComponent offValue;
    NumberInputComponent onValue;
    
    juce::GroupComponent realtimeMsgGroup;
    DropdownComponent realtimeOff;
    DropdownComponent realtimeOn;

    juce::ListenerList<Listener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMessageSectionComponent)
};
