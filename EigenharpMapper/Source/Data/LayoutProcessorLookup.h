#pragma once
#include <JuceHeader.h>
#include "../Models/MappedKey.h"
#include "OSCCommunication.h"
#include "OSCMessageQueue.h"

class LayoutProcessorLookup : public juce::ValueTree::Listener {
public:
    LayoutProcessorLookup(OSC::OSCMessageFifo *oscSendQueue);
private:
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property);
    void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree);
    void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex);
    void valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex);
    void valueTreeParentChanged(juce::ValueTree &vTree);
    void valueTreeRedirected(juce::ValueTree &vTree);
    OSC::OSCMessageFifo *oscSendQueue;
};
