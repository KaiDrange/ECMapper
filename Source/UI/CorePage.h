#pragma once
#include <JuceHeader.h>
#include "../Core/HardwareService.h"

namespace ecm {

class CorePage : public juce::Component, private juce::Timer {
public:
    CorePage(HardwareService& hardwareService);
    ~CorePage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }

    HardwareService& hardwareService_;
    juce::Image ledGreen;
    juce::Image ledOff;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorePage)
};

} // namespace ecm
