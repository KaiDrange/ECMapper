#pragma once
#include <JuceHeader.h>

namespace ecm {

class TabButtonBarComponent : public juce::TabbedComponent {
public:
    TabButtonBarComponent() : TabbedComponent(juce::TabbedButtonBar::TabsAtTop) {
        onTabChanged = [](int) {};
        setTabBarDepth(34);
        setOutline(1);
        setIndent(0);
    }

    void currentTabChanged(int index, const juce::String& name) override {
        juce::ignoreUnused(name);
        onTabChanged(index);
    }

    std::function<void(int)> onTabChanged;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabButtonBarComponent)
};

} // namespace ecm
