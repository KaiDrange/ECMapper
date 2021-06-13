#include "LayoutChangeHandler.h"

LayoutChangeHandler::LayoutChangeHandler(OSC::OSCMessageFifo *oscSendQueue, ECMapperAudioProcessor *processor, ConfigLookup (&configLookups) [3]) {
    this->configLookups = configLookups;
    this->oscSendQueue = oscSendQueue;
    this->processor = processor;
}

void LayoutChangeHandler::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    processor->suspendProcessing(true);
    DeviceType deviceType = DeviceType::None;
    if (vTree.getType().toString().startsWith(LayoutWrapper::id_key + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(vTree);
        deviceType = layoutKey.keyId.deviceType;
        
        if (deviceType != DeviceType::None) {
            if (property == LayoutWrapper::id_keyColour)
                sendLEDMsg(layoutKey);
            else
                configLookups[getConfigIndexFromDeviceType(deviceType)].updateKey(vTree);
        }
    }
    else if (vTree.getParent().getType().toString().startsWith(ZoneWrapper::id_zone)) {
        DeviceType deviceType = ZoneWrapper::getDeviceTypeFromTree(vTree);
        configLookups[((int)deviceType) - 1].updateAll();
    }
    else if (vTree.getType().toString().startsWith(ZoneWrapper::id_zone)) {
        DeviceType deviceType = ZoneWrapper::getDeviceTypeFromTree(vTree);
        configLookups[((int)deviceType) - 1].updateAll();
    }

//    if (deviceType != DeviceType::None) {
//        layoutMidiRPNSent = false;
//    }
    processor->suspendProcessing(false);
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
    OSC::Message msg {
        .type = OSC::MessageType::Reset,
        .key = 0,
        .course = 0,
        .active = 0,
        .pressure = 0,
        .roll = 0,
        .yaw = 0,
        .value = 0,
        .pedal = 0,
        .strip = 0,
        .device = deviceType
    };
    oscSendQueue->add(&msg);


    auto layoutTree = LayoutWrapper::getLayoutTree(deviceType, processor->pluginState.state);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(layoutTree.getChild(i));
        if (layoutKey.keyColour != KeyColour::Off)
            sendLEDMsg(layoutKey);
    }
}

void LayoutChangeHandler::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {
    if (childTree.getType().toString().startsWith(LayoutWrapper::id_key + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(childTree);
        if (layoutKey.keyId.deviceType != DeviceType::None) {
            sendLEDMsg(layoutKey);
        }
    }
}

void LayoutChangeHandler::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutChangeHandler::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutChangeHandler::valueTreeParentChanged(juce::ValueTree &vTree) {}

void LayoutChangeHandler::valueTreeRedirected(juce::ValueTree &vTree) {
    for (int i = 0; i < 3; i++)
        configLookups[i].updateAll();
}
