#pragma once
#include <JuceHeader.h>

class NumberInputComponent  : public juce::Component {
public:
    NumberInputComponent(const juce::String labelText, const int maxDigits, const int minValue, const int maxValue, const int defaultValue);
    ~NumberInputComponent() override;

    int getValue();
    void setLabelText(juce::String text);
    void resized() override;
    
private:
    juce::Label label;
    juce::TextEditor input;
    int maxValue;
    int minValue;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NumberInputComponent)
};
