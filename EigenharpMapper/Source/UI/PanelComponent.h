#pragma once

#include <JuceHeader.h>

class PanelComponent  : public juce::Component {
public:
    PanelComponent(float widthFactor, float heightFactor);
    ~PanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    float widthFactor;
    float heightFactor;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelComponent)
};
