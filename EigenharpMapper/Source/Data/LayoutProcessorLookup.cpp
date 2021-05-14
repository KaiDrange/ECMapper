#include "LayoutProcessorLookup.h"

LayoutProcessorLookup::LayoutProcessorLookup(OSC::OSCMessageFifo *oscSendQueue) {
    this->oscSendQueue = oscSendQueue;
}

void LayoutProcessorLookup::setActiveLayout(Layout *layout) {
    this->layout = layout;
    updateLookup();
}

void LayoutProcessorLookup::updateLookup() {
    KeyConfig *keyConfigs = layout->getKeyConfigs();
    for (int i = 0; i < layout->getNormalkeyCount(); i++) {
        Key key;
        key.mapType = keyConfigs[i].getMappingType();
        key.zone = keyConfigs[i].getZone();
        key.note = key.mapType == KeyMappingType::Note ? keyConfigs[i].getMappingValue().getIntValue() : 0;
        lookup[0][i] = key;
    }
        
    if (layout->getInstrumentType() == InstrumentType::Pico) {
        //TODO: rest of courses
    }
    
}

void LayoutProcessorLookup::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    if (vTree.getType().toString() == "key") {
        auto key = KeyConfig(vTree);
        auto keyId = key.getKeyId();
        OSC::Message msg {
            .type = OSC::MessageType::LED,
            .key = (unsigned int)keyId.keyNo,
            .course = (unsigned int)keyId.course,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .value = key.getKeyColour(),
            .pedal = 0,
            .strip = 0
        };
        oscSendQueue->add(&msg);
    }
    updateLookup();
    
}

void LayoutProcessorLookup::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {}
void LayoutProcessorLookup::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutProcessorLookup::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutProcessorLookup::valueTreeParentChanged(juce::ValueTree &vTree) {}
void LayoutProcessorLookup::valueTreeRedirected(juce::ValueTree &vTree) {}
