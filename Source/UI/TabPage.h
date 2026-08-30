#pragma once
#include <JuceHeader.h>
#include "LayoutComponent.h"
#include "ZonePanelComponent.h"
#include "../Core/Enums.h"
#include "../Core/SettingsWrapper.h"

namespace ecm {

class TabPage : public juce::Component {
public:
    TabPage(int tabIndex, InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState);
    ~TabPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    InstrumentType deviceType;
    std::unique_ptr<LayoutComponent> layoutPanel;
    std::unique_ptr<ZonePanelComponent> zonePanels[3];

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton saveMappingButton { "Save" };
    juce::TextButton loadMappingButton { "Load" };
    juce::TextButton clearMappingButton { "Clear" };
    
    juce::AudioProcessorValueTreeState& pluginState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};

} // namespace ecm
