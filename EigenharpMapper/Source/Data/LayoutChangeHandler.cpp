#include "LayoutChangeHandler.h"

LayoutChangeHandler::LayoutChangeHandler(OSC::OSCMessageFifo *oscSendQueue) {
    this->oscSendQueue = oscSendQueue;
}

void LayoutChangeHandler::setKeyConfigLookup(KeyConfigLookup *keyConfigLookup, DeviceType deviceType) {
    this->keyConfigLookups[getConfigIndexFromDeviceType(deviceType)] = keyConfigLookup;
}

void LayoutChangeHandler::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    DeviceType deviceType = DeviceType::None;
    auto vTreeType = vTree.getType().toString();
    if (vTreeType.startsWith("key")) {
        
        KeyConfig key(vTree);
        deviceType = (DeviceType)vTree.getParent().getPropertyAsValue("instrumentType", nullptr).toString().getIntValue();
        OSC::Message msg {
            .type = OSC::MessageType::LED,
            .key = (unsigned int)key.getKeyId().keyNo,
            .course = (unsigned int)key.getKeyId().course,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .value = (unsigned int)key.getKeyColour(),
            .pedal = 0,
            .strip = 0,
            .device = deviceType
        };
        oscSendQueue->add(&msg);
    }

    if (deviceType != DeviceType::None) {
        keyConfigLookups[getConfigIndexFromDeviceType(deviceType)]->updateAll();
        layoutMidiRPNSent = false;
    }
}

int LayoutChangeHandler::getConfigIndexFromDeviceType(DeviceType type) {
    return (int)type - 1;
}

void LayoutChangeHandler::sendLEDMsgForAllKeys(juce::ValueTree layoutTree) {
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {

        KeyConfig key(layoutTree.getChild(i));
        DeviceType deviceType = (DeviceType)layoutTree.getPropertyAsValue("instrumentType", nullptr).toString().getIntValue();
        OSC::Message msg {
            .type = OSC::MessageType::LED,
            .key = (unsigned int)key.getKeyId().keyNo,
            .course = (unsigned int)key.getKeyId().course,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .value = (unsigned int)key.getKeyColour(),
            .pedal = 0,
            .strip = 0,
            .device = deviceType
        };
        oscSendQueue->add(&msg);
    }
}

void LayoutChangeHandler::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {}
void LayoutChangeHandler::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutChangeHandler::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutChangeHandler::valueTreeParentChanged(juce::ValueTree &vTree) {}
void LayoutChangeHandler::valueTreeRedirected(juce::ValueTree &vTree) {}
