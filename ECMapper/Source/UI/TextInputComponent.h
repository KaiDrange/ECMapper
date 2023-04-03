#pragma once
#include <JuceHeader.h>

class TextInputComponent : public juce::Component {
public:
    TextInputComponent(const juce::String labelText, const int minLength, const int maxLength, const juce::String legalChars, const bool labelAboveInput);
    ~TextInputComponent() override;

    juce::String getValue();
    void setValue(juce::String text);
    void setLabelText(juce::String text);
    void resized() override;
    
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void textInputChanged(TextInputComponent*) = 0;
    };
    void addListener(Listener *listenerToAdd);
    void removeListener(Listener *listenerToRemove);
    juce::TextEditor input;

private:
    void sendChangeMessage();
    juce::Label label;
    int minLength;
    int maxLength;
    juce::String legalChars;
    juce::ListenerList<Listener> listeners;
    bool labelAboveInput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextInputComponent)
};
