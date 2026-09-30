#pragma once

#include <JuceHeader.h>
#include "ExpressionCurveEditorComponent.h"

namespace ecm {
class MidiService;

class ExpressionCurvesComponent : public juce::Component {
public:
    ExpressionCurvesComponent(InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState, MidiService& midiService);
    void resized() override;
    void refreshFromState();

private:
    InstrumentType deviceType_;
    juce::AudioProcessorValueTreeState& pluginState_;
    juce::ToggleButton showLiveDots { "Show live dots" };
    std::unique_ptr<ExpressionCurveEditorComponent> editors[6];
};

} // namespace ecm
