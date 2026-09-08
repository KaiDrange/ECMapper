#pragma once

#include <algorithm>
#include <memory>
#include "PerformanceEvent.h"

namespace ecm {

struct ExpressionEmissionConfig {
    int minMessageStride = 1;
    float minNormalizedDelta = 0.0f;
    bool quantizeToTransportResolution = false;
};

class ExpressionEmissionPolicy {
public:
    explicit ExpressionEmissionPolicy(ExpressionEmissionConfig config);
    virtual ~ExpressionEmissionPolicy() = default;

    virtual bool shouldEmitContinuousUpdate(int messageCount) const;
    const ExpressionEmissionConfig& getConfig() const { return config_; }

protected:
    ExpressionEmissionConfig config_;
};

std::shared_ptr<ExpressionEmissionPolicy> createExpressionEmissionPolicy(OutputTransportMode mode);

}