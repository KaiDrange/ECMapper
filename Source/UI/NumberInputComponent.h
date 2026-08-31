#pragma once
#include <JuceHeader.h>

namespace ecm {

class NumberInputComponent  : public juce::Component {
public:
    NumberInputComponent(const juce::String& labelText, int maxDigits, int minValue, int maxValue, bool labelAboveInput);
    ~NumberInputComponent() override = default;

    int getValue() const;
    void setValue(int number);
    void setLabelText(const juce::String& text);
    void resized() override;
    void enablementChanged() override;
    
    struct Listener {
        virtual ~Listener() = default;
        virtual void numberInputChanged(NumberInputComponent*) = 0;
    };
    void addListener(Listener* listenerToAdd);
    void removeListener(Listener* listenerToRemove);
    juce::TextEditor input;

private:
    void sendChangeMessage();
    void updateTextColours();
    juce::Label label;
    int maxValue;
    int minValue;
    juce::ListenerList<Listener> listeners;
    bool labelAboveInput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NumberInputComponent)
};

} // namespace ecm
