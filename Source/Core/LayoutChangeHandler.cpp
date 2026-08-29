#include "LayoutChangeHandler.h"
#include "ZoneWrapper.h"
#include "SettingsWrapper.h"

namespace ecm {

LayoutChangeHandler::LayoutChangeHandler(osc::MessageFifo& oscSendQueue, 
                                        juce::ValueTree& state, 
                                        ConfigLookup (&configLookups)[3],
                                        std::function<void(bool)> suspendProcessingCallback)
    : oscSendQueue_(oscSendQueue), state_(state), configLookups_(configLookups), suspendProcessingCallback_(suspendProcessingCallback) {
}

void LayoutChangeHandler::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    if (suspendProcessingCallback_) suspendProcessingCallback_(true);
    
    InstrumentType deviceType = InstrumentType::None;
    auto typeStr = vTree.getType().toString();
    
    if (typeStr.startsWith(LayoutWrapper::id_key.toString() + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(vTree);
        deviceType = layoutKey.keyId.deviceType;
        
        if (deviceType != InstrumentType::None) {
            int configIndex = getConfigIndexFromInstrumentType(deviceType);
            if (property == LayoutWrapper::id_keyColour && configLookups_[configIndex].controlLights) {
                sendLEDMsg(layoutKey);
            } else {
                configLookups_[configIndex].updateKey(vTree);
            }
        }
    } else if (vTree.getParent().getType().toString().startsWith(ZoneWrapper::id_zone.toString())) {
        deviceType = ZoneWrapper::getInstrumentTypeFromTree(vTree);
        if (deviceType != InstrumentType::None) {
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    } else if (typeStr.startsWith(ZoneWrapper::id_zone.toString())) {
        deviceType = ZoneWrapper::getInstrumentTypeFromTree(vTree);
        if (deviceType != InstrumentType::None) {
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    } else if (property == SettingsWrapper::id_controlLights && typeStr.startsWith(LayoutWrapper::id_device.toString())) {
        deviceType = static_cast<InstrumentType>(typeStr.substring(6).getIntValue());
        if (deviceType != InstrumentType::None) {
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].controlLights = SettingsWrapper::getControlLights(deviceType, state_);
            sendLEDMsgForAllKeys(deviceType);
        }
    }

    if (suspendProcessingCallback_) suspendProcessingCallback_(false);
}

void LayoutChangeHandler::sendLEDMsg(LayoutWrapper::LayoutKey layoutKey) {
    if (layoutKey.keyId.deviceType == InstrumentType::None) return;

    int configIndex = getConfigIndexFromInstrumentType(layoutKey.keyId.deviceType);
    if (!configLookups_[configIndex].controlLights) return;
    
    osc::Message msg;
    msg.type = osc::MessageType::LED;
    msg.key = static_cast<unsigned int>(layoutKey.keyId.keyNo);
    msg.course = static_cast<unsigned int>(layoutKey.keyId.course);
    msg.value = static_cast<unsigned int>(layoutKey.keyColour);
    msg.device = layoutKey.keyId.deviceType;
    
    oscSendQueue_.add(msg);
}

void LayoutChangeHandler::sendLEDMsgForAllKeys(InstrumentType deviceType) {
    if (deviceType == InstrumentType::None) return;

    int configIndex = getConfigIndexFromInstrumentType(deviceType);
    if (!configLookups_[configIndex].controlLights) return;

    osc::Message msg;
    msg.type = osc::MessageType::Reset;
    msg.device = deviceType;
    oscSendQueue_.add(msg);

    auto layoutTree = LayoutWrapper::getLayoutTree(deviceType, state_);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(layoutTree.getChild(i));
        if (layoutKey.keyColour != KeyColour::Off) {
            sendLEDMsg(layoutKey);
        }
    }
}

void LayoutChangeHandler::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree& childTree) {
    if (childTree.getType().toString().startsWith(LayoutWrapper::id_key.toString() + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(childTree);
        if (layoutKey.keyId.deviceType != InstrumentType::None) {
            sendLEDMsg(layoutKey);
        }
    }
}

void LayoutChangeHandler::valueTreeRedirected(juce::ValueTree&) {
    for (int i = 0; i < 3; i++) {
        configLookups_[i].updateAll();
    }
}

} // namespace ecm
