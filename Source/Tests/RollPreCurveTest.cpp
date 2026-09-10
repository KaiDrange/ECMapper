#include <cmath>
#include <iostream>

#include "Core/ExpressionPreprocessor.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool expectNear(float actual, float expected, float tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << message << " expected=" << expected << " actual=" << actual << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main() {
    using ecm::applyRollPreCurve;

    bool ok = true;
    ok &= expectNear(applyRollPreCurve(0.0f), 0.0f, 1.0e-6f, "zero should stay centered");
    ok &= expectNear(applyRollPreCurve(1.0f), 1.0f, 1.0e-6f, "positive extreme should stay unchanged");
    ok &= expectNear(applyRollPreCurve(-1.0f), -1.0f, 1.0e-6f, "negative extreme should stay unchanged");

    const float quarter = applyRollPreCurve(0.25f);
    const float minusQuarter = applyRollPreCurve(-0.25f);
    ok &= expectNear(minusQuarter, -quarter, 1.0e-6f, "curve should stay symmetric around zero");
    ok &= expect(std::abs(quarter) < 0.25f, "quarter-scale motion should compress toward center");

    const float half = applyRollPreCurve(0.5f);
    ok &= expect(std::abs(half) < 0.5f, "half-scale motion should still be softened before the visual curve");
    ok &= expect(std::abs(half) > std::abs(quarter), "curve should remain monotonic away from center");

    if (!ok)
        return 1;

    std::cout << "RollPreCurveTest passed" << std::endl;
    return 0;
}