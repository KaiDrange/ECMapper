#include "BezierCurve.h"

BezierCurve::BezierCurve(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    this->x0 = x0;
    this->y0 = y0;
    this->x1 = x1;
    this->y1 = y1;
    this->x2 = x2;
    this->y2 = y2;
    this->x3 = x3;
    this->y3 = y3;
    
    createTable();
}

float BezierCurve::getCurvePoint(float n1, float n2, int tablePos) {
    float perc = tablePos/(TABLE_LENGTH*1.0f);
    float diff = n2 - n1;
    return n1 + (diff*perc);
}

void BezierCurve::createTable() {
    for(int i = 0; i < TABLE_LENGTH; i++)
    {
        //float xa = getCurvePoint(x0, x1, i);
        float ya = getCurvePoint(y0, y1, i);
        //float xb = getCurvePoint(x1, x2, i);
        float yb = getCurvePoint(y1, y2, i);
        //float xc = getCurvePoint(x2, x3, i);
        float yc = getCurvePoint(y2, y3, i);

        //float xm = getCurvePoint( xa, xb, i);
        float ym = getCurvePoint( ya, yb, i);
        float yn = getCurvePoint( yb, yc, i);

        table[i] = getCurvePoint(ym, yn, i);
    }
    table[TABLE_LENGTH-1] = y3;
}

float BezierCurve::getTableValue(int index) {
    index = std::min(TABLE_LENGTH-1, std::max(0, index));
    return table[index];
}
