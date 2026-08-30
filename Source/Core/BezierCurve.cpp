#include "BezierCurve.h"
#include <algorithm>

namespace ecm {

BezierCurve::BezierCurve(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
    : x0(x0), y0(y0), x1(x1), y1(y1), x2(x2), y2(y2), x3(x3), y3(y3) {
    createTable();
}

float BezierCurve::getCurvePoint(float n1, float n2, int tablePos) const noexcept {
    float perc = tablePos / (TABLE_LENGTH * 1.0f);
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

        table[i] = getCurvePoint(ym, yn, i);
    }
    table[TABLE_LENGTH - 1] = y3;
}

float BezierCurve::getTableValue(int index) const noexcept {
    index = std::clamp(index, 0, TABLE_LENGTH - 1);
    return table[index];
}

} // namespace ecm
