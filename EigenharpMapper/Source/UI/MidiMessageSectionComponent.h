#pragma once
#include <JuceHeader.h>
#include "NumberInputComponent.h"

class MidiMessageSectionComponent  : public juce::Component
{
public:
    MidiMessageSectionComponent();
    ~MidiMessageSectionComponent() override;

    void resized() override;
    juce::String getMessageString();

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void valuesChanged(MidiMessageSectionComponent*) = 0;
    };
    void addListener(Listener *listenerToAdd);
    void removeListener(Listener *listenerToRemove);

private:
    void sendChangeMessage();
    
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
    juce::ListenerList<Listener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMessageSectionComponent)
};
