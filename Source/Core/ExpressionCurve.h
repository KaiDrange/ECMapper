#pragma once

#include <array>
#include <JuceHeader.h>

namespace ecm {

enum class ExpressionCurveTarget {
    Breath = 0,
    Velocity = 1,
    ReleaseVelocity = 2,
    Pressure = 3,
    Yaw = 4,
    Roll = 5
};

struct ExpressionCurvePoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct ExpressionCurveData {
    float startY = 0.0f;
    ExpressionCurvePoint leftControl { 0.25f, 0.25f };
    float centerY = 0.5f;
    ExpressionCurvePoint rightControl { 0.75f, 0.75f };
    float endY = 1.0f;
};

class ExpressionCurve {
public:
    static constexpr int TABLE_LENGTH = 1024;

    ExpressionCurve();
    explicit ExpressionCurve(const ExpressionCurveData& data);

    void setData(const ExpressionCurveData& data);
    const ExpressionCurveData& getData() const { return data_; }

    float getValue(float normalizedInput) const noexcept;
    juce::Path createPath(juce::Rectangle<float> bounds) const;

    static ExpressionCurveData defaultData(ExpressionCurveTarget target);

private:
    ExpressionCurveData data_;

    static float quadratic(float p0, float p1, float p2, float t) noexcept;
    static float solveQuadraticForX(float p0, float p1, float p2, float x) noexcept;
};

} // namespace ecm
