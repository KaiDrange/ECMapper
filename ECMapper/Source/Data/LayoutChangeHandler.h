#pragma once
#include <JuceHeader.h>
#include "OSCCommunication.h"
#include "OSCMessageQueue.h"
#include "../Models/Enums.h"
#include "../Models/LayoutWrapper.h"
#include "ConfigLookup.h"

class ECMapperAudioProcessor;

extern juce::ValueTree *rootState;

class LayoutChangeHandler : public juce::ValueTree::Listener {
public:
    LayoutChangeHandler(OSC::OSCMessageFifo *oscSendQueue, ECMapperAudioProcessor *processor, ConfigLookup (&configLookups) [3]);
    void sendLEDMsg(LayoutWrapper::LayoutKey layoutKey);
    void sendLEDMsgForAllKeys(DeviceType deviceType);
    bool layoutMidiRPNSent = false;

private:
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property);
    void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree);
    void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex);
    void valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex);
    void valueTreeParentChanged(juce::ValueTree &vTree);
    void valueTreeRedirected(juce::ValueTree &vTree);
    
    OSC::OSCMessageFifo *oscSendQueue;
    
    int getConfigIndexFromDeviceType(DeviceType type);
    ConfigLookup *configLookups;
    ECMapperAudioProcessor *processor;
};

#include "../PluginProcessor.h"
