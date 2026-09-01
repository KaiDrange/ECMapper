#pragma once

#include <JuceHeader.h>
#include "ExpressionCurveEditorComponent.h"

namespace ecm {

class ExpressionCurvesComponent : public juce::Component {
public:
    ExpressionCurvesComponent(InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState);
    void resized() override;
    void refreshFromState();

private:
    std::unique_ptr<ExpressionCurveEditorComponent> editors[6];
};

} // namespace ecm
