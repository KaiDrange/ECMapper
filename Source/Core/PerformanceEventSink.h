#pragma once

#include <JuceHeader.h>
#include "MidiProtocol.h"

namespace ecm {

class PerformanceEventSink {
public:
    virtual ~PerformanceEventSink() = default;
    virtual void pushEvent(const PerformanceEvent& event) = 0;
};

class MidiBufferPerformanceEventSink final : public PerformanceEventSink {
public:
    MidiBufferPerformanceEventSink(juce::MidiBuffer& buffer, MidiProtocol* protocol)
        : buffer_(buffer), protocol_(protocol) {
    }

    void pushEvent(const PerformanceEvent& event) override {
        if (protocol_ != nullptr)
            protocol_->renderEvent(buffer_, event);
    }

private:
    juce::MidiBuffer& buffer_;
    MidiProtocol* protocol_ = nullptr;
};

}