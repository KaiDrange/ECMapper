#include "LayoutWrapper.h"

juce::ValueTree LayoutWrapper::getLayoutTree(DeviceType deviceType, juce::ValueTree &rootState) {
    auto deviceChild = rootState.getOrCreateChildWithName(id_device + juce::String((int)deviceType), nullptr);
    return deviceChild.getOrCreateChildWithName(id_layout, nullptr);
}

juce::ValueTree LayoutWrapper::getKeyTree(KeyId keyId, juce::ValueTree &rootState) {
    auto layoutTree = getLayoutTree(keyId.deviceType, rootState);
    return layoutTree.getOrCreateChildWithName(id_key + "_" + juce::String(keyId.course) + "_" + juce::String(keyId.keyNo), nullptr);
}

void LayoutWrapper::addListener(DeviceType deviceType, juce::ValueTree::Listener *listener, juce::ValueTree &rootState) {
    auto vTree = getLayoutTree(deviceType, rootState);
    vTree.addListener(listener);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKey(KeyId keyId, juce::ValueTree &rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    auto defaultKeyType = getCorrectDefaultKeyType(keyId.deviceType, keyId.course, keyId.keyNo);
    
    return LayoutKey {
        .keyId = keyId,
        .keyType = (EigenharpKeyType)int(keyTree.getProperty(id_keyType,(int)defaultKeyType)),
        .keyColour = (KeyColour)int(keyTree.getProperty(id_keyColour, (int)default_key.keyColour)),
        .zone = (Zone)int(keyTree.getProperty(id_zone, (int)default_key.zone)),
        .keyMappingType = (KeyMappingType)int(keyTree.getProperty(id_keyMappingType, (int)getDefaultMappingTypeFromKeyType(defaultKeyType))),
        .mappingValue = keyTree.getProperty(id_mappingValue, defaultKeyType == EigenharpKeyType::Normal ? "0" : "")
    };
}

void LayoutWrapper::setLayoutKey(LayoutKey &key, juce::ValueTree &rootState) {
    setKeyColour(key.keyId, key.keyColour, rootState);
    setKeyType(key.keyId, key.keyType, rootState);
    setKeyZone(key.keyId, key.zone, rootState);
    setKeyMappingType(key.keyId, key.keyMappingType, rootState);
    setKeyMappingValue(key.keyId, key.mappingValue, rootState);
}

void LayoutWrapper::setKeyColour(KeyId &keyId, KeyColour keyColour, juce::ValueTree &rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyColour, (int)keyColour, nullptr);
}

void LayoutWrapper::setKeyType(KeyId &keyId, EigenharpKeyType keyType, juce::ValueTree &rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyType, (int)keyType, nullptr);
}

void LayoutWrapper::setKeyZone(KeyId &keyId, Zone zone, juce::ValueTree &rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_zone, (int)zone, nullptr);
}

void LayoutWrapper::setKeyMappingType(KeyId &keyId, KeyMappingType keyMappingType, juce::ValueTree &rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyMappingType, (int)keyMappingType, nullptr);
}

void LayoutWrapper::setKeyMappingValue(KeyId &keyId, juce::String keyMappingValue, juce::ValueTree &rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_mappingValue, keyMappingValue, nullptr);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKeyFromKeyTree(juce::ValueTree keyTree) {
    if (!keyTree.isValid())
        return default_key;
    DeviceType deviceType = getDeviceTypeFromKeyTree(keyTree);
    int course = keyTree.getType().toString().substring(4, 5).getIntValue();
    int keyNo = keyTree.getType().toString().substring(6).getIntValue();
    KeyId keyId = {
        .deviceType = deviceType,
        .course = course,
        .keyNo = keyNo
    };
    
    auto rootState = keyTree.getRoot();
    return getLayoutKey(keyId, rootState);
}

DeviceType LayoutWrapper::getDeviceTypeFromKeyTree(juce::ValueTree keyTree) {
    if (!keyTree.isValid())
        return DeviceType::None;

    return (DeviceType)keyTree.getParent().getParent().getType().toString().substring(6, 7).getIntValue();
}

DeviceType LayoutWrapper::getDeviceTypeFromLayoutTree(juce::ValueTree layoutTree) {
    if (!layoutTree.isValid())
        return DeviceType::None;

    return (DeviceType)layoutTree.getParent().getType().toString().substring(6, 7).getIntValue();
}

EigenharpKeyType LayoutWrapper::getCorrectDefaultKeyType(DeviceType deviceType, int course, int keyNo) {
    switch (deviceType) {
        case DeviceType::Alpha:
            return course == 0 ? EigenharpKeyType::Normal : EigenharpKeyType::Perc;
            break;
        case DeviceType::Tau:
            if (course == 0 && keyNo < 72) return EigenharpKeyType::Normal;
            else if (course == 0 && keyNo >= 72) return EigenharpKeyType::Perc;
            else return EigenharpKeyType::Button;
            break;
        case DeviceType::Pico:
            return course == 0 ? EigenharpKeyType::Normal : EigenharpKeyType::Button;
            break;
        default:
            return EigenharpKeyType::Normal;
    }
}

KeyMappingType LayoutWrapper::getDefaultMappingTypeFromKeyType(EigenharpKeyType keyType) {
    return keyType == EigenharpKeyType::Normal ? KeyMappingType::Note : KeyMappingType::None;
}
