#include "LayoutWrapper.h"

namespace ecm {

juce::ValueTree LayoutWrapper::getLayoutTree(InstrumentType deviceType, juce::ValueTree& rootState) {
    auto deviceChild = rootState.getOrCreateChildWithName(id_device + juce::String((int)deviceType), nullptr);
    return deviceChild.getOrCreateChildWithName(id_layout, nullptr);
}

juce::ValueTree LayoutWrapper::getKeyTree(KeyId keyId, juce::ValueTree& rootState) {
    auto layoutTree = getLayoutTree(keyId.deviceType, rootState);
    return layoutTree.getOrCreateChildWithName(id_key + "_" + juce::String(keyId.course) + "_" + juce::String(keyId.keyNo), nullptr);
}

void LayoutWrapper::addListener(InstrumentType deviceType, juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    auto vTree = getLayoutTree(deviceType, rootState);
    vTree.addListener(listener);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKey(KeyId keyId, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    auto defaultKeyType = getCorrectDefaultKeyType(keyId.deviceType, keyId.course, keyId.keyNo);
    
    return LayoutKey {
        .keyId = keyId,
        .keyType = (EigenharpKeyType)int(keyTree.getProperty(id_keyType, (int)defaultKeyType)),
        .keyColour = (KeyColour)int(keyTree.getProperty(id_keyColour, (int)KeyColour::Off)),
        .zone = (Zone)int(keyTree.getProperty(id_zone, (int)Zone::Zone1)),
        .keyMappingType = (KeyMappingType)int(keyTree.getProperty(id_keyMappingType, (int)getDefaultMappingTypeFromKeyType(defaultKeyType))),
        .mappingValue = keyTree.getProperty(id_mappingValue, defaultKeyType == EigenharpKeyType::Normal ? "0" : "")
    };
}

void LayoutWrapper::setLayoutKey(LayoutKey& key, juce::ValueTree& rootState) {
    setKeyColour(key.keyId, key.keyColour, rootState);
    setKeyType(key.keyId, key.keyType, rootState);
    setKeyZone(key.keyId, key.zone, rootState);
    setKeyMappingType(key.keyId, key.keyMappingType, rootState);
    setKeyMappingValue(key.keyId, key.mappingValue, rootState);
}

void LayoutWrapper::setKeyColour(KeyId keyId, KeyColour keyColour, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyColour, (int)keyColour, nullptr);
}

void LayoutWrapper::setKeyType(KeyId keyId, EigenharpKeyType keyType, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyType, (int)keyType, nullptr);
}

void LayoutWrapper::setKeyZone(KeyId keyId, Zone zone, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_zone, (int)zone, nullptr);
}

void LayoutWrapper::setKeyMappingType(KeyId keyId, KeyMappingType keyMappingType, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyMappingType, (int)keyMappingType, nullptr);
}

void LayoutWrapper::setKeyMappingValue(KeyId keyId, juce::String keyMappingValue, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_mappingValue, keyMappingValue, nullptr);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKeyFromKeyTree(juce::ValueTree keyTree) {
    if (!keyTree.isValid())
        return LayoutKey { .keyId = {0,0,InstrumentType::None}, .keyType = EigenharpKeyType::Normal, .keyColour = KeyColour::Off, .zone = Zone::Zone1, .keyMappingType = KeyMappingType::Note, .mappingValue = "0" };
    
    InstrumentType deviceType = getInstrumentTypeFromKeyTree(keyTree);
    
    auto typeStr = keyTree.getType().toString();
    auto parts = juce::StringArray::fromTokens(typeStr, "_", "");
    int course = (parts.size() > 1) ? parts[1].getIntValue() : 0;
    int keyNo = (parts.size() > 2) ? parts[2].getIntValue() : 0;
    
    KeyId keyId = {
        .course = course,
        .keyNo = keyNo,
        .deviceType = deviceType
    };
    
    auto rootState = keyTree.getRoot();
    return getLayoutKey(keyId, rootState);
}

InstrumentType LayoutWrapper::getInstrumentTypeFromKeyTree(juce::ValueTree keyTree) {
    if (!keyTree.isValid() || !keyTree.getParent().isValid() || !keyTree.getParent().getParent().isValid())
        return InstrumentType::None;

    return (InstrumentType)keyTree.getParent().getParent().getType().toString().substring(6).getIntValue();
}

InstrumentType LayoutWrapper::getInstrumentTypeFromLayoutTree(juce::ValueTree layoutTree) {
    if (!layoutTree.isValid() || !layoutTree.getParent().isValid())
        return InstrumentType::None;

    return (InstrumentType)layoutTree.getParent().getType().toString().substring(6).getIntValue();
}

EigenharpKeyType LayoutWrapper::getCorrectDefaultKeyType(InstrumentType deviceType, int course, int keyNo) {
    switch (deviceType) {
        case InstrumentType::Alpha:
            return course == 0 ? EigenharpKeyType::Normal : EigenharpKeyType::Perc;
        case InstrumentType::Tau:
            if (course == 0 && keyNo < 72) return EigenharpKeyType::Normal;
            else if (course == 0 && keyNo >= 72) return EigenharpKeyType::Perc;
            else return EigenharpKeyType::Button;
        case InstrumentType::Pico:
            return course == 0 ? EigenharpKeyType::Normal : EigenharpKeyType::Button;
        default:
            return EigenharpKeyType::Normal;
    }
}

KeyMappingType LayoutWrapper::getDefaultMappingTypeFromKeyType(EigenharpKeyType keyType) {
    return keyType == EigenharpKeyType::Normal ? KeyMappingType::Note : KeyMappingType::None;
}

} // namespace ecm
