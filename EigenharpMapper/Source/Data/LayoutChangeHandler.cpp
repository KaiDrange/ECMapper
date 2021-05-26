#include "LayoutChangeHandler.h"

LayoutChangeHandler::LayoutChangeHandler(OSC::OSCMessageFifo *oscSendQueue) {
    this->oscSendQueue = oscSendQueue;
}

void LayoutChangeHandler::setKeyConfigLookup(KeyConfigLookup *keyConfigLookup, DeviceType deviceType) {
    this->keyConfigLookups[getConfigIndexFromDeviceType(deviceType)] = keyConfigLookup;
}

void LayoutChangeHandler::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    DeviceType deviceType = DeviceType::None;
    if (vTree.getType().toString().startsWith(LayoutWrapper::id_key + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(vTree);
        deviceType = layoutKey.keyId.deviceType;
        
        OSC::Message msg {
            .type = OSC::MessageType::LED,
            .key = (unsigned int)layoutKey.keyId.keyNo,
            .course = (unsigned int)layoutKey.keyId.course,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .value = (unsigned int)layoutKey.keyColour,
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

void LayoutChangeHandler::sendLEDMsg(LayoutWrapper::LayoutKey layoutKey) {
    if (layoutKey.keyId.deviceType == DeviceType::None)
        return;
    
    OSC::Message msg {
        .type = OSC::MessageType::LED,
        .key = (unsigned int)layoutKey.keyId.keyNo,
        .course = (unsigned int)layoutKey.keyId.course,
        .active = 0,
        .pressure = 0,
        .roll = 0,
        .yaw = 0,
        .value = (unsigned int)layoutKey.keyColour,
        .pedal = 0,
        .strip = 0,
        .device = layoutKey.keyId.deviceType
    };
    oscSendQueue->add(&msg);
}

void LayoutChangeHandler::sendLEDMsgForAllKeys(DeviceType deviceType) {
    auto layoutTree = LayoutWrapper::getLayoutTree(deviceType);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(layoutTree.getChild(i));
        sendLEDMsg(layoutKey);
    }
}

void LayoutChangeHandler::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {
    if (childTree.getType().toString().startsWith(LayoutWrapper::id_key + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(childTree);
        if (layoutKey.keyId.deviceType != DeviceType::None)
            sendLEDMsg(layoutKey);
    }
}

void LayoutChangeHandler::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutChangeHandler::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutChangeHandler::valueTreeParentChanged(juce::ValueTree &vTree) {}
void LayoutChangeHandler::valueTreeRedirected(juce::ValueTree &vTree) {}
