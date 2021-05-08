#pragma once

#include <JuceHeader.h>
#include "PanelComponent.h"

class ZonePanelComponent  : public PanelComponent {
public:
    ZonePanelComponent(int zoneNumber, float widthFactor, float heightFactor);
    ~ZonePanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int zoneNumber;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZonePanelComponent)
};
