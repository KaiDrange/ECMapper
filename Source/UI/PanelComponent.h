#pragma once
#include <JuceHeader.h>

namespace ecm {

class PanelComponent  : public juce::Component {
public:
    PanelComponent(float widthFactor, float heightFactor);
    ~PanelComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    float widthFactor;
    float heightFactor;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelComponent)
};

} // namespace ecm
