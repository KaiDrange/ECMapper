#pragma once
#include <JuceHeader.h>
#include "../Models/KeyConfig.h"
#include "OSCCommunication.h"
#include "OSCMessageQueue.h"
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "KeyConfigLookup.h"

class LayoutChangeHandler : public juce::ValueTree::Listener {
public:
    LayoutChangeHandler(OSC::OSCMessageFifo *oscSendQueue);
    void setKeyConfigLookup(KeyConfigLookup *keyConfigLookup);
    
    bool layoutMidiRPNSent = false;

private:
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property);
    void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree);
    void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex);
    void valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex);
    void valueTreeParentChanged(juce::ValueTree &vTree);
    void valueTreeRedirected(juce::ValueTree &vTree);
    
    OSC::OSCMessageFifo *oscSendQueue;
    KeyConfigLookup *keyConfigLookup;
};
