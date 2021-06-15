#pragma once

#include <JuceHeader.h>

class TabButtonBarComponent : public juce::TabbedComponent
{
public:
    TabButtonBarComponent() : TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    {
        onTabChanged = [](int) {};
    }
    void currentTabChanged(int index, const juce::String&) override
    {
        onTabChanged(index);
    }
    std::function<void(int)> onTabChanged;
};
