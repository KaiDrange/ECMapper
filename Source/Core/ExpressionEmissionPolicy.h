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

inline bool usesIndependentPerNoteExpression(OutputTransportMode mode, bool remoteSupportsPerNote)
{
    switch (mode) {
        case OutputTransportMode::UmpMidi:
            return remoteSupportsPerNote;
        case OutputTransportMode::Vst3Direct:
            return false;
        case OutputTransportMode::LegacyMidi:
            return false;
    }

    return false;
}

inline bool shouldUsePerNoteExpressionEvent(OutputTransportMode mode, bool remoteSupportsPerNote, int noteNumber)
{
    return noteNumber != -1 && usesIndependentPerNoteExpression(mode, remoteSupportsPerNote);
}

std::shared_ptr<ExpressionEmissionPolicy> createExpressionEmissionPolicy(OutputTransportMode mode);

}