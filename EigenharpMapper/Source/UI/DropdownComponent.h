#pragma once
#include <JuceHeader.h>

class DropdownComponent  : public juce::Component {
public:
    DropdownComponent();
    ~DropdownComponent() override;

    void resized() override;
    void setLabelText(const juce::String text);
    void addItem(const juce::String text, const int itemId);
    void setSelectedItemId(int id);

private:
    juce::Label label;
    juce::ComboBox box;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropdownComponent)
};
