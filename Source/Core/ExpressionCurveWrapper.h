#pragma once

#include <JuceHeader.h>
#include "Enums.h"
#include "ExpressionCurve.h"

namespace ecm {

class ExpressionCurveWrapper {
public:
    static inline const juce::Identifier id_device { "device" };
    static inline const juce::Identifier id_expressionCurves { "expressionCurves" };
    static inline const juce::Identifier id_curve { "curve" };
    static inline const juce::Identifier id_startY { "startY" };
    static inline const juce::Identifier id_leftX { "leftX" };
    static inline const juce::Identifier id_leftY { "leftY" };
    static inline const juce::Identifier id_centerY { "centerY" };
    static inline const juce::Identifier id_rightX { "rightX" };
    static inline const juce::Identifier id_rightY { "rightY" };
    static inline const juce::Identifier id_endY { "endY" };

    static ExpressionCurve getCurve(InstrumentType deviceType, ExpressionCurveTarget target, juce::ValueTree& rootState);
    static void setCurve(InstrumentType deviceType, ExpressionCurveTarget target, const ExpressionCurve& curve, juce::ValueTree& rootState);
    static InstrumentType getInstrumentTypeFromTree(juce::ValueTree tree);

private:
    static juce::ValueTree getDeviceTree(InstrumentType deviceType, juce::ValueTree& rootState, bool create);
    static juce::ValueTree getExpressionCurvesTree(InstrumentType deviceType, juce::ValueTree& rootState, bool create);
    static juce::ValueTree getCurveTree(InstrumentType deviceType, ExpressionCurveTarget target, juce::ValueTree& rootState, bool create);
    static ExpressionCurveData getCurveDataFromTree(juce::ValueTree curveTree, ExpressionCurveTarget target);
};

} // namespace ecm
