#include "ExpressionCurve.h"

#include <algorithm>

namespace ecm {

ExpressionCurve::ExpressionCurve()
    : data_(defaultData(ExpressionCurveTarget::Pressure)) {
}

ExpressionCurve::ExpressionCurve(const ExpressionCurveData& data)
    : data_(data) {
}

void ExpressionCurve::setData(const ExpressionCurveData& data) {
    data_ = data;
}

float ExpressionCurve::quadratic(float p0, float p1, float p2, float t) noexcept {
    auto u = 1.0f - t;
    return (u * u * p0) + (2.0f * u * t * p1) + (t * t * p2);
}

float ExpressionCurve::solveQuadraticForX(float p0, float p1, float p2, float x) noexcept {
    float low = 0.0f;
    float high = 1.0f;

    for (int i = 0; i < 20; ++i) {
        float mid = (low + high) * 0.5f;
        float midX = quadratic(p0, p1, p2, mid);
        if (midX < x)
            low = mid;
        else
            high = mid;
    }

    return (low + high) * 0.5f;
}

float ExpressionCurve::getValue(float normalizedInput) const noexcept {
    normalizedInput = std::clamp(normalizedInput, 0.0f, 1.0f);

    const auto leftStartX = 0.0f;
    const auto leftControlX = std::clamp(data_.leftControl.x, 0.0f, 0.5f);
    const auto centerX = 0.5f;
    const auto rightControlX = std::clamp(data_.rightControl.x, 0.5f, 1.0f);
    const auto endX = 1.0f;
    const auto startY = std::clamp(data_.startY, 0.0f, 1.0f);
    const auto endY = std::clamp(data_.endY, 0.0f, 1.0f);

    if (normalizedInput <= 0.5f) {
        auto x = normalizedInput;
        auto t = solveQuadraticForX(leftStartX, leftControlX, centerX, x);
        return quadratic(startY, data_.leftControl.y, data_.centerY, t);
    }

    auto x = normalizedInput;
    auto t = solveQuadraticForX(centerX, rightControlX, endX, x);
    return quadratic(data_.centerY, data_.rightControl.y, endY, t);
}

juce::Path ExpressionCurve::createPath(juce::Rectangle<float> bounds) const {
    juce::Path path;
    auto toPoint = [bounds](float x, float y) {
        return juce::Point<float>(
            bounds.getX() + bounds.getWidth() * x,
            bounds.getBottom() - bounds.getHeight() * y
        );
    };

    auto leftStart = toPoint(0.0f, std::clamp(data_.startY, 0.0f, 1.0f));
    auto center = toPoint(0.5f, std::clamp(data_.centerY, 0.0f, 1.0f));
    auto end = toPoint(1.0f, std::clamp(data_.endY, 0.0f, 1.0f));
    auto leftControl = toPoint(std::clamp(data_.leftControl.x, 0.0f, 0.5f), std::clamp(data_.leftControl.y, 0.0f, 1.0f));
    auto rightControl = toPoint(std::clamp(data_.rightControl.x, 0.5f, 1.0f), std::clamp(data_.rightControl.y, 0.0f, 1.0f));

    path.startNewSubPath(leftStart);
    path.quadraticTo(leftControl.x, leftControl.y, center.x, center.y);
    path.quadraticTo(rightControl.x, rightControl.y, end.x, end.y);
    return path;
}

ExpressionCurveData ExpressionCurve::defaultData(ExpressionCurveTarget target) {
    switch (target) {
        case ExpressionCurveTarget::Velocity:
            return { 0.0f, {0.25f, 0.25f}, 0.5f, {0.75f, 0.75f}, 1.0f };
        case ExpressionCurveTarget::ReleaseVelocity:
            return { 0.0f, {0.25f, 0.30f}, 0.50f, {0.75f, 0.72f}, 1.0f };
        case ExpressionCurveTarget::Breath:
            return { 0.0f, {0.25f, 0.28f}, 0.52f, {0.75f, 0.78f}, 1.0f };
        case ExpressionCurveTarget::Pressure:
            return { 0.0f, {0.25f, 0.25f}, 0.50f, {0.75f, 0.75f}, 1.0f };
        case ExpressionCurveTarget::Yaw:
            return { 0.0f, {0.25f, 0.26f}, 0.50f, {0.75f, 0.74f}, 1.0f };
        case ExpressionCurveTarget::Roll:
            return { 0.0f, {0.25f, 0.26f}, 0.50f, {0.75f, 0.74f}, 1.0f };
        default:
            return { 0.0f, {0.25f, 0.25f}, 0.50f, {0.75f, 0.75f}, 1.0f };
    }
}

} // namespace ecm
