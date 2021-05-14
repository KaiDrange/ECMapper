#pragma once
#include <JuceHeader.h>
#include "../Models/KeyConfig.h"
#include "OSCCommunication.h"
#include "OSCMessageQueue.h"
#include "../Models/Enums.h"

class LayoutProcessorLookup : public juce::ValueTree::Listener {
public:
    LayoutProcessorLookup(OSC::OSCMessageFifo *oscSendQueue);
    
    void setUISettingsPointer(juce::ValueTree *uiSettings);
private:
    void valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property);
    void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree);
    void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex);
    void valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex);
    void valueTreeParentChanged(juce::ValueTree &vTree);
    void valueTreeRedirected(juce::ValueTree &vTree);
    
    void updateLookup();
    OSC::OSCMessageFifo *oscSendQueue;
    juce::ValueTree *uiSettings;
    
    enum KeyStatus {
        Off = 0,
        Pending = 1,
        Active = 2
    };
    
    struct Key {
        KeyMappingType mapType = KeyMappingType::None;
        int note = 0;
        KeyStatus status = KeyStatus::Off;
        unsigned int pressure = 0;
        unsigned int roll = 0;
        unsigned int yaw = 0;
        int midiChannel = -1;
        Zone zone = Zone::NoZone;
    };
    
    Key lookup[3][120];
};
