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
    std::unique_ptr<ExpressionCurveEditorComponent> editors[6];
};

} // namespace ecm
