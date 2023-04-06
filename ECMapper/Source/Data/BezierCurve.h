#pragma once
#include <JuceHeader.h>

class BezierCurve {
public:
    BezierCurve(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
    static const int TABLE_LENGTH = 1024;
    
    void createTable();
    float getTableValue(int index);
private:
    float table[TABLE_LENGTH];
    float x0, y0, x1, y1, x2, y2, x3, y3 = 0.0;
    
    float getCurvePoint(float n1, float n2, int tablePos);
};
