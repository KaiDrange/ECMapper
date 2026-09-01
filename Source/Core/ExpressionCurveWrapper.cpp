#include "ExpressionCurveWrapper.h"

namespace ecm {

juce::ValueTree ExpressionCurveWrapper::getDeviceTree(InstrumentType deviceType, juce::ValueTree& rootState, bool create) {
    auto deviceName = id_device + juce::String((int)deviceType);
    return create ? rootState.getOrCreateChildWithName(deviceName, nullptr)
                  : rootState.getChildWithName(deviceName);
}

juce::ValueTree ExpressionCurveWrapper::getExpressionCurvesTree(InstrumentType deviceType, juce::ValueTree& rootState, bool create) {
    auto deviceTree = getDeviceTree(deviceType, rootState, create);
    if (!deviceTree.isValid())
        return {};

    return create ? deviceTree.getOrCreateChildWithName(id_expressionCurves, nullptr)
                  : deviceTree.getChildWithName(id_expressionCurves);
}

juce::ValueTree ExpressionCurveWrapper::getCurveTree(InstrumentType deviceType, ExpressionCurveTarget target, juce::ValueTree& rootState, bool create) {
    auto curvesTree = getExpressionCurvesTree(deviceType, rootState, create);
    if (!curvesTree.isValid())
        return {};

    auto curveName = id_curve + juce::String((int)target);
    return create ? curvesTree.getOrCreateChildWithName(curveName, nullptr)
                  : curvesTree.getChildWithName(curveName);
}

ExpressionCurveData ExpressionCurveWrapper::getCurveDataFromTree(juce::ValueTree curveTree, ExpressionCurveTarget target) {
    auto defaults = ExpressionCurve::defaultData(target);

    if (!curveTree.isValid())
        return defaults;

    return {
        .startY = curveTree.getProperty(id_startY, defaults.startY),
        .leftControl = {
            curveTree.getProperty(id_leftX, defaults.leftControl.x),
            curveTree.getProperty(id_leftY, defaults.leftControl.y)
        },
        .centerY = curveTree.getProperty(id_centerY, defaults.centerY),
        .rightControl = {
            curveTree.getProperty(id_rightX, defaults.rightControl.x),
            curveTree.getProperty(id_rightY, defaults.rightControl.y)
        },
        .endY = curveTree.getProperty(id_endY, defaults.endY)
    };
}

ExpressionCurve ExpressionCurveWrapper::getCurve(InstrumentType deviceType, ExpressionCurveTarget target, juce::ValueTree& rootState) {
    auto curveTree = getCurveTree(deviceType, target, rootState, false);
    return ExpressionCurve(getCurveDataFromTree(curveTree, target));
}

void ExpressionCurveWrapper::setCurve(InstrumentType deviceType, ExpressionCurveTarget target, const ExpressionCurve& curve, juce::ValueTree& rootState) {
    auto curveTree = getCurveTree(deviceType, target, rootState, true);
    auto data = curve.getData();
    curveTree.setProperty(id_startY, data.startY, nullptr);
    curveTree.setProperty(id_leftX, data.leftControl.x, nullptr);
    curveTree.setProperty(id_leftY, data.leftControl.y, nullptr);
    curveTree.setProperty(id_centerY, data.centerY, nullptr);
    curveTree.setProperty(id_rightX, data.rightControl.x, nullptr);
    curveTree.setProperty(id_rightY, data.rightControl.y, nullptr);
    curveTree.setProperty(id_endY, data.endY, nullptr);
}

InstrumentType ExpressionCurveWrapper::getInstrumentTypeFromTree(juce::ValueTree tree) {
    auto parentTree = tree.getParent();
    while (parentTree.isValid() && !parentTree.getType().toString().startsWith(id_device.toString()))
        parentTree = parentTree.getParent();

    if (parentTree.isValid())
        return (InstrumentType)parentTree.getType().toString().substring(6).getIntValue();

    return InstrumentType::None;
}

} // namespace ecm
