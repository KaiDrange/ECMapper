#include "ExpressionPreprocessor.h"

#include <algorithm>
#include <cmath>

namespace ecm {

float applyRollPreCurve(float value) noexcept {
    value = std::clamp(value, -1.0f, 1.0f);

    constexpr float rollCenterPower = 1.5f;
    const float magnitude = std::pow(std::abs(value), rollCenterPower);
    return std::copysign(magnitude, value);
}

} // namespace ecm