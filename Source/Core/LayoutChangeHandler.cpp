#include "LayoutChangeHandler.h"
#include "ExpressionCurveWrapper.h"
#include "ZoneWrapper.h"
#include "SettingsWrapper.h"

namespace ecm {

LayoutChangeHandler::LayoutChangeHandler(osc::MessageFifo& oscSendQueue,
                                         juce::ValueTree& state,
                                         ConfigLookup (&configLookups)[3],
                                         juce::CriticalSection& stateLock,
                                         std::function<bool()> shouldSuppressNotificationsCallback,
                                         std::function<void(InstrumentType, Zone)> zoneChangeCallback)
    : oscSendQueue_(oscSendQueue), state_(state), configLookups_(configLookups), stateLock_(stateLock), shouldSuppressNotificationsCallback_(shouldSuppressNotificationsCallback), zoneChangeCallback_(zoneChangeCallback) {
}

Zone LayoutChangeHandler::getZoneFromTree(juce::ValueTree& vTree) {
    auto typeStr = vTree.getType().toString();
    if (!typeStr.startsWith(ZoneWrapper::id_zone.toString()))
        return Zone::NoZone;

    return static_cast<Zone>(typeStr.substring(4).getIntValue());
}

void LayoutChangeHandler::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    const juce::ScopedLock stateGuard(stateLock_);
    if (shouldSuppressNotificationsCallback_ && shouldSuppressNotificationsCallback_())
        return;

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
            auto zone = getZoneFromTree(vTree);
            if ((property == ZoneWrapper::id_transpose ||
                 (property == ZoneWrapper::id_enabled && !ZoneWrapper::getEnabled(deviceType, zone, state_))) &&
                zoneChangeCallback_) {
                zoneChangeCallback_(deviceType, zone);
            }
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    } else if (vTree.getParent().getType().toString().startsWith(ExpressionCurveWrapper::id_expressionCurves.toString())) {
        deviceType = ExpressionCurveWrapper::getInstrumentTypeFromTree(vTree);
        if (deviceType != InstrumentType::None) {
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    } else if (typeStr.startsWith(ZoneWrapper::id_zone.toString())) {
        deviceType = ZoneWrapper::getInstrumentTypeFromTree(vTree);
        if (deviceType != InstrumentType::None) {
            auto zone = getZoneFromTree(vTree);
            if ((property == ZoneWrapper::id_transpose ||
                 (property == ZoneWrapper::id_enabled && !ZoneWrapper::getEnabled(deviceType, zone, state_))) &&
                zoneChangeCallback_) {
                zoneChangeCallback_(deviceType, zone);
            }
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    } else if (typeStr.startsWith(ExpressionCurveWrapper::id_expressionCurves.toString()) || typeStr.startsWith(ExpressionCurveWrapper::id_curve.toString())) {
        deviceType = ExpressionCurveWrapper::getInstrumentTypeFromTree(vTree);
        if (deviceType != InstrumentType::None) {
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    }

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
    const juce::ScopedLock stateGuard(stateLock_);
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
    const juce::ScopedLock stateGuard(stateLock_);
    if (shouldSuppressNotificationsCallback_ && shouldSuppressNotificationsCallback_())
        return;

    if (childTree.getType().toString().startsWith(LayoutWrapper::id_key.toString() + "_")) {
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(childTree);
        if (layoutKey.keyId.deviceType != InstrumentType::None) {
            sendLEDMsg(layoutKey);
        }
    } else if (childTree.getType().toString().startsWith(ExpressionCurveWrapper::id_expressionCurves.toString()) ||
               childTree.getType().toString().startsWith(ExpressionCurveWrapper::id_curve.toString())) {
        auto deviceType = ExpressionCurveWrapper::getInstrumentTypeFromTree(childTree);
        if (deviceType != InstrumentType::None) {
            configLookups_[getConfigIndexFromInstrumentType(deviceType)].updateAll();
        }
    }
}

void LayoutChangeHandler::valueTreeRedirected(juce::ValueTree&) {
    const juce::ScopedLock stateGuard(stateLock_);
    if (shouldSuppressNotificationsCallback_ && shouldSuppressNotificationsCallback_())
        return;

    for (int i = 0; i < 3; i++) {
        configLookups_[i].updateAll();
    }
}

} // namespace ecm
