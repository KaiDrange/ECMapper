#pragma once
#include <JuceHeader.h>
#include "LayoutComponent.h"
#include "ZonePanelComponent.h"
#include "ExpressionCurvesComponent.h"
#include "../Core/Enums.h"

namespace ecm {

class TabPage : public juce::Component {
public:
    TabPage(int tabIndex, InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState);
    ~TabPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class RightPanelView {
        Curves,
        Zones
    };

    void setRightPanelView(RightPanelView view);

    InstrumentType deviceType;
    int tabIndex_;
    std::unique_ptr<LayoutComponent> layoutPanel;
    std::unique_ptr<ExpressionCurvesComponent> expressionCurvesComponent;
    std::unique_ptr<ZonePanelComponent> zonePanels[3];
    juce::TextButton curvesViewButton { "Curves" };
    juce::TextButton zonesViewButton { "Zones" };
    RightPanelView rightPanelView = RightPanelView::Curves;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton saveMappingButton { "Save" };
    juce::TextButton loadMappingButton { "Load" };
    juce::TextButton clearMappingButton { "Clear" };
    
    juce::AudioProcessorValueTreeState& pluginState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};

} // namespace ecm
