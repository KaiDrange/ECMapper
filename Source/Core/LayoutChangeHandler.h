#pragma once
#include <JuceHeader.h>
#include "OSCMessage.h"
#include "LayoutWrapper.h"
#include "ConfigLookup.h"

namespace ecm {

class LayoutChangeHandler : public juce::ValueTree::Listener {
public:
    LayoutChangeHandler(osc::MessageFifo& oscSendQueue, 
                        juce::ValueTree& state, 
                        ConfigLookup (&configLookups)[3],
                        std::function<void(bool)> suspendProcessingCallback);
    
    void sendLEDMsg(LayoutWrapper::LayoutKey layoutKey);
    void sendLEDMsgForAllKeys(InstrumentType deviceType);
    
    bool layoutMidiRPNSent = false;

private:
    void valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) override;
    void valueTreeChildAdded(juce::ValueTree& parentTree, juce::ValueTree& childTree) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged(juce::ValueTree&) override {}
    void valueTreeRedirected(juce::ValueTree& vTree) override;
    
    osc::MessageFifo& oscSendQueue_;
    juce::ValueTree& state_;
    ConfigLookup (&configLookups_)[3];
    std::function<void(bool)> suspendProcessingCallback_;
    
    int getConfigIndexFromInstrumentType(InstrumentType type) { return static_cast<int>(type) - 1; }
};

} // namespace ecm
