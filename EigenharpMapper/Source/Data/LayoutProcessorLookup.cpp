#include "LayoutProcessorLookup.h"

LayoutProcessorLookup::LayoutProcessorLookup(OSC::OSCMessageFifo *oscSendQueue) {
    this->oscSendQueue = oscSendQueue;
}

void LayoutProcessorLookup::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    if (vTree.getType().toString() == "key") {
        auto key = MappedKey(vTree);
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
}

void LayoutProcessorLookup::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {}
void LayoutProcessorLookup::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutProcessorLookup::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutProcessorLookup::valueTreeParentChanged(juce::ValueTree &vTree) {}
void LayoutProcessorLookup::valueTreeRedirected(juce::ValueTree &vTree) {}
