#include "KeyConfig.h"

KeyConfig::KeyConfig(KeyConfig::KeyId keyId, EigenharpKeyType keyType, juce::ValueTree parentTree) {
    juce::Identifier id = id_key + juce::String(keyId.course) + "_" + juce::String(keyId.keyNo);
    bool alreadyExists = parentTree.getChildWithName(id).isValid();
    valueTree = parentTree.getOrCreateChildWithName(id, nullptr);

    if (!alreadyExists) {
        setKeyId(keyId);
        setKeyColour(KeyColour::Off);
        setKeyType(keyType);
        setZone(Zone::NoZone);
        keyType == EigenharpKeyType::Button ?
            setMappingType(KeyMappingType::None) :
            setMappingType(KeyMappingType::Note);
    }

//    rootValueTree.addChild(valueTree, -1, nullptr);
}

KeyConfig::KeyConfig(juce::ValueTree keyTree) {
    valueTree = keyTree;
}

juce::ValueTree KeyConfig::getValueTree() const {
    return valueTree;
}

//void KeyConfig::setValueTree(juce::ValueTree valueTree) {
//    this->valueTree.copyPropertiesFrom(valueTree, nullptr);
//}

EigenharpKeyType KeyConfig::getKeyType() const {
    return (EigenharpKeyType)valueTree[id_keyType].toString().getIntValue();
}

void KeyConfig::setKeyType(EigenharpKeyType keyType) {
    valueTree.setProperty(id_keyType, (int)keyType, nullptr);
}

KeyColour KeyConfig::getKeyColour() const {
    return (KeyColour)valueTree[id_keyColour].toString().getIntValue();
}

void KeyConfig::setKeyColour(KeyColour colour) {
    valueTree.setProperty(id_keyColour, (int)colour, nullptr);
}

Zone KeyConfig::getZone() const {
    return (Zone)valueTree[id_zone].toString().getIntValue();
}

void KeyConfig::setZone(Zone zone) {
    valueTree.setProperty(id_zone, (int)zone, nullptr);
}

KeyMappingType KeyConfig::getMappingType() const {
    return (KeyMappingType)valueTree[id_keyMappingType].toString().getIntValue();
}

void KeyConfig::setMappingType(KeyMappingType mappingType) {
    valueTree.setProperty(id_keyMappingType, (int)mappingType, nullptr);
    mappingType == KeyMappingType::Note ?
        setMappingValue("24") :
    setMappingValue("");
}

juce::String KeyConfig::getMappingValue() const {
    return valueTree[id_mappingValue].toString();
}

void KeyConfig::setMappingValue(juce::String mappingValue) {
    valueTree.setProperty(id_mappingValue, mappingValue, nullptr);
}

KeyConfig::KeyId KeyConfig::getKeyId() const {
    return KeyId {
        .keyNo = valueTree.getProperty(id_keyNo),
        .course = valueTree.getProperty(id_course)
    };
}

void KeyConfig::setKeyId(KeyId keyId) {
    valueTree.setProperty(id_keyNo, keyId.keyNo, nullptr);
    valueTree.setProperty(id_course, keyId.course, nullptr);
}


