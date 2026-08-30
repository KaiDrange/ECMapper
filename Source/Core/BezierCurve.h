#pragma once
#include <array>
#include <JuceHeader.h>

namespace ecm {

class BezierCurve {
public:
    BezierCurve(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3);
    static constexpr int TABLE_LENGTH = 1024;
    
    void createTable();
    float getTableValue(int index) const noexcept;
    
private:
    std::array<float, TABLE_LENGTH> table;
    float x0, y0, x1, y1, x2, y2, x3, y3;
    
    float getCurvePoint(float n1, float n2, int tablePos) const noexcept;
};

} // namespace ecm
