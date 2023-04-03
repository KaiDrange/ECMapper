#pragma once
#include <JuceHeader.h>
#include "TextInputComponent.h"

class ChordSectionComponent : public juce::Component, TextInputComponent::Listener
{
public:
    ChordSectionComponent();
    ~ChordSectionComponent() override;

    void resized() override;
    juce::String getMessageString();
    void updatePanelFromMessageString(juce::String msgString);

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void valuesChanged(ChordSectionComponent*) = 0;
    };
    void addListener(Listener *listenerToAdd);
    void removeListener(Listener *listenerToRemove);
    
    struct ChordNote {
        juce::Label label;
        juce::TextButton setButton;
        juce::TextButton clearButton;
    };

private:
    void sendChangeMessage();
    void textInputChanged(TextInputComponent*) override;
    void resetPanel();
    TextInputComponent chordNameInput;
    ChordNote chordNotes[4];

    juce::ListenerList<Listener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSectionComponent)
};
