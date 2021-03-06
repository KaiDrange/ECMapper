#pragma once
#include <JuceHeader.h>

class NumberInputComponent  : public juce::Component {
public:
    NumberInputComponent(const juce::String labelText, const int maxDigits, const int minValue, const int maxValue, const bool labelAboveInput);
    ~NumberInputComponent() override;

    int getValue();
    void setValue(int number);
    void setLabelText(juce::String text);
    void resized() override;
    
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void numberInputChanged(NumberInputComponent*) = 0;
    };
    void addListener(Listener *listenerToAdd);
    void removeListener(Listener *listenerToRemove);
    juce::TextEditor input;

private:
    void sendChangeMessage();
    juce::Label label;
    int maxValue;
    int minValue;
    juce::ListenerList<Listener> listeners;
    bool labelAboveInput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NumberInputComponent)
};
