#include "ExpressionEmissionPolicy.h"

namespace ecm {

ExpressionEmissionPolicy::ExpressionEmissionPolicy(ExpressionEmissionConfig config)
    : config_(config) {
}

bool ExpressionEmissionPolicy::shouldEmitContinuousUpdate(int messageCount) const {
    return messageCount >= std::max(1, config_.minMessageStride);
}

std::shared_ptr<ExpressionEmissionPolicy> createExpressionEmissionPolicy(OutputTransportMode mode) {
    switch (mode) {
        case OutputTransportMode::LegacyMidi:
            return std::make_shared<ExpressionEmissionPolicy>(ExpressionEmissionConfig { 64, 0.0f, true, 250 });
        case OutputTransportMode::UmpMidi:
            return std::make_shared<ExpressionEmissionPolicy>(ExpressionEmissionConfig { 32, 0.0f, false, 250 });
        case OutputTransportMode::Vst3Direct:
            return std::make_shared<ExpressionEmissionPolicy>(ExpressionEmissionConfig { 1, 0.0f, false, 250 });
    }

    return std::make_shared<ExpressionEmissionPolicy>(ExpressionEmissionConfig {});
}

}