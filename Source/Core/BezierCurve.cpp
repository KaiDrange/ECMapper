#include "BezierCurve.h"
#include <algorithm>

namespace ecm {

BezierCurve::BezierCurve(float, float startY, float, float controlY1, float, float controlY2, float, float endY)
    : y0(startY), y1(controlY1), y2(controlY2), y3(endY) {
    createTable();
}

float BezierCurve::getCurvePoint(float n1, float n2, int tablePos) const noexcept {
    float perc = static_cast<float>(tablePos) / static_cast<float>(TABLE_LENGTH);
    float diff = n2 - n1;
    return n1 + (diff * perc);
}

void BezierCurve::createTable() {
    for (int i = 0; i < TABLE_LENGTH; i++) {
        float ya = getCurvePoint(y0, y1, i);
        float yb = getCurvePoint(y1, y2, i);
        float yc = getCurvePoint(y2, y3, i);

        float ym = getCurvePoint(ya, yb, i);
        float yn = getCurvePoint(yb, yc, i);

        table[static_cast<std::size_t>(i)] = getCurvePoint(ym, yn, i);
    }
    table[TABLE_LENGTH - 1] = y3;
}

float BezierCurve::getTableValue(int index) const noexcept {
    index = std::clamp(index, 0, TABLE_LENGTH - 1);
    return table[static_cast<std::size_t>(index)];
}

} // namespace ecm
