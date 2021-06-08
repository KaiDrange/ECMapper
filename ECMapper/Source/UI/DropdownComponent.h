#pragma once
#include <JuceHeader.h>

class DropdownComponent  : public juce::Component {
public:
    DropdownComponent();
    ~DropdownComponent() override;

    void resized() override;
    void setLabelText(const juce::String text, const bool labelAboveBox);
    void addItem(const juce::String text, const int itemId);
    void setSelectedItemId(int id);
    juce::ComboBox box;

private:
    juce::Label label;
    bool labelAbove;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropdownComponent)
};
