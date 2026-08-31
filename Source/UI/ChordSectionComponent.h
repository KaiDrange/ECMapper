#pragma once
#include <JuceHeader.h>
#include "TextInputComponent.h"

namespace ecm {

class ChordSectionComponent : public juce::Component, public TextInputComponent::Listener, public juce::MidiKeyboardStateListener {
public:
    ChordSectionComponent();
    ~ChordSectionComponent() override;

    void resized() override;
    juce::String getMessageString() const;
    void updatePanelFromMessageString(const juce::String& msgString);

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    
    struct Listener {
        virtual ~Listener() = default;
        virtual void valuesChanged(ChordSectionComponent*) = 0;
    };
    void addListener(Listener* listenerToAdd) { listeners.add(listenerToAdd); }
    void removeListener(Listener* listenerToRemove) { listeners.remove(listenerToRemove); }
    void textInputChanged(TextInputComponent*) override;
    void visibilityChanged() override;

    struct ChordNote {
        juce::Label label;
        juce::TextButton setButton { "Set" };
        juce::TextButton clearButton { "Clear" };
        int midiNoteNumber = -1;
    };



private:
    void setNoteLabelText(int noteIndex);
    void sendChangeMessage();
    void resetPanel();
    
    TextInputComponent chordNameInput;
    ChordNote chordNotes[4];
    int noteSettingIndex = -1;

    juce::ListenerList<Listener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordSectionComponent)
};

} // namespace ecm
