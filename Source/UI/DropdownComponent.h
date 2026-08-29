#pragma once
#include <JuceHeader.h>

namespace ecm {

class DropdownComponent  : public juce::Component {
public:
    DropdownComponent();
    ~DropdownComponent() override = default;

    void resized() override;
    void setLabelText(const juce::String& text, bool labelAboveBox);
    void addItem(const juce::String& text, int itemId);
    void setSelectedItemId(int id);
    juce::ComboBox box;

private:
    juce::Label label;
    bool labelAbove = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DropdownComponent)
};

} // namespace ecm
