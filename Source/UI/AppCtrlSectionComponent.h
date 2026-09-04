#pragma once
#include <JuceHeader.h>
#include "NumberInputComponent.h"

namespace ecm {

class AppCtrlSectionComponent  : public juce::Component, public NumberInputComponent::Listener {
public:
    AppCtrlSectionComponent();
    ~AppCtrlSectionComponent() override = default;

    void resized() override;
    juce::String getMessageString();
    void updatePanelFromMessageString(const juce::String& msgString);

    struct Listener {
        virtual ~Listener() = default;
        virtual void valuesChanged(AppCtrlSectionComponent*) = 0;
    };
    void addListener(Listener* listenerToAdd) { listeners.add(listenerToAdd); }
    void removeListener(Listener* listenerToRemove) { listeners.remove(listenerToRemove); }

private:
    void sendChangeMessage();
    void numberInputChanged(NumberInputComponent*) override;
    void visibilityChanged() override;
    
    juce::GroupComponent typeRadioGroup;
    juce::ToggleButton typePreset;
    juce::ToggleButton typeTranspose;

    juce::GroupComponent modeRadioGroup;
    juce::ToggleButton modeLatch;
    juce::ToggleButton modeMomentary;
    juce::ToggleButton modeTrigger;

    NumberInputComponent presetNumber;
    NumberInputComponent transposeSemiTones;

    juce::ListenerList<Listener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppCtrlSectionComponent)
};

} // namespace ecm
