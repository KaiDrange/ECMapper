#include "LayoutWrapper.h"

juce::ValueTree LayoutWrapper::getLayoutTree(DeviceType deviceType) {
    auto deviceChild = rootState->getOrCreateChildWithName(id_device + juce::String((int)deviceType), nullptr);
    return deviceChild.getOrCreateChildWithName(id_layout, nullptr);
}

juce::ValueTree LayoutWrapper::getKeyTree(KeyId keyId) {
    auto layoutTree = getLayoutTree(keyId.deviceType);
    return layoutTree.getOrCreateChildWithName(id_key + "_" + juce::String(keyId.course) + "_" + juce::String(keyId.keyNo), nullptr);
}

void LayoutWrapper::addListener(DeviceType deviceType, juce::ValueTree::Listener *listener) {
    auto vTree = getLayoutTree(deviceType);
    vTree.addListener(listener);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKey(KeyId keyId) {
    auto keyTree = getKeyTree(keyId);
    
    return LayoutKey {
        .keyId = keyId,
        .keyType = (EigenharpKeyType)int(keyTree.getProperty(id_keyType, (int)default_key.keyType)),
        .keyColour = (KeyColour)int(keyTree.getProperty(id_keyColour, (int)default_key.keyColour)),
        .zone = (Zone)int(keyTree.getProperty(id_zone, (int)default_key.zone)),
        .keyMappingType = (KeyMappingType)int(keyTree.getProperty(id_keyMappingType, (int)default_key.keyMappingType)),
        .mappingValue = keyTree.getProperty(id_mappingValue, default_key.mappingValue)
    };
}

void LayoutWrapper::setLayoutKey(LayoutKey &key) {
    setKeyColour(key.keyId, key.keyColour);
    setKeyType(key.keyId, key.keyType);
    setKeyZone(key.keyId, key.zone);
    setKeyMappingType(key.keyId, key.keyMappingType);
    setKeyMappingValue(key.keyId, key.mappingValue);
}

void LayoutWrapper::setKeyColour(KeyId &keyId, KeyColour keyColour) {
    auto keyTree = getKeyTree(keyId);
    keyTree.setProperty(id_keyColour, (int)keyColour, nullptr);
}

void LayoutWrapper::setKeyType(KeyId &keyId, EigenharpKeyType keyType) {
    auto keyTree = getKeyTree(keyId);
    keyTree.setProperty(id_keyType, (int)keyType, nullptr);
}

void LayoutWrapper::setKeyZone(KeyId &keyId, Zone zone) {
    auto keyTree = getKeyTree(keyId);
    keyTree.setProperty(id_zone, (int)zone, nullptr);
}

void LayoutWrapper::setKeyMappingType(KeyId &keyId, KeyMappingType keyMappingType) {
    auto keyTree = getKeyTree(keyId);
    keyTree.setProperty(id_keyMappingType, (int)keyMappingType, nullptr);
}

void LayoutWrapper::setKeyMappingValue(KeyId &keyId, juce::String keyMappingValue) {
    auto keyTree = getKeyTree(keyId);
    keyTree.setProperty(id_mappingValue, keyMappingValue, nullptr);
}
