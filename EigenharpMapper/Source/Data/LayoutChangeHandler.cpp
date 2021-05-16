#include "LayoutChangeHandler.h"

LayoutChangeHandler::LayoutChangeHandler(OSC::OSCMessageFifo *oscSendQueue) {
    this->oscSendQueue = oscSendQueue;
}

void LayoutChangeHandler::setKeyConfigLookup(KeyConfigLookup *keyConfigLookup) {
    this->keyConfigLookup = keyConfigLookup;
}

void LayoutChangeHandler::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
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
    keyConfigLookup->updateAll();
    layoutMidiRPNSent = false;
}

void LayoutChangeHandler::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {}
void LayoutChangeHandler::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutChangeHandler::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutChangeHandler::valueTreeParentChanged(juce::ValueTree &vTree) {}
void LayoutChangeHandler::valueTreeRedirected(juce::ValueTree &vTree) {}
