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
                        juce::CriticalSection& stateLock,
                        std::function<bool()> shouldSuppressNotificationsCallback,
                        std::function<void(InstrumentType, Zone)> zoneChangeCallback = {});
    
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
    juce::CriticalSection& stateLock_;
    std::function<bool()> shouldSuppressNotificationsCallback_;
    std::function<void(InstrumentType, Zone)> zoneChangeCallback_;
    
    int getConfigIndexFromInstrumentType(InstrumentType type) {
        auto index = static_cast<int>(type) - 1;
        jassert(index >= 0 && index < 3);
        return index;
    }
    static Zone getZoneFromTree(juce::ValueTree& vTree);
};

} // namespace ecm
