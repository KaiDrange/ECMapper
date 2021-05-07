#include "MappedKey.h"

MappedKey::MappedKey(EigenharpKeyType keyType, juce::ValueTree &rootValueTree): valueTree("key") {
    setKeyColour(KeyColour::Off);
    setKeyType(keyType);
    setZone(Zone::NoZone);
    keyType == EigenharpKeyType::Button ?
        setMappingType(KeyMappingType::None) :
        setMappingType(KeyMappingType::Note);

    rootValueTree.addChild(valueTree, -1, nullptr);
}

juce::ValueTree MappedKey::getValueTree() const {
    return valueTree;
}

void MappedKey::setValueTree(juce::ValueTree valueTree) {
    this->valueTree = valueTree;
}

EigenharpKeyType MappedKey::getKeyType() const {
    return (EigenharpKeyType)valueTree[id_keyType].toString().getIntValue();
}

void MappedKey::setKeyType(EigenharpKeyType keyType) {
    valueTree.setProperty(id_keyType, keyType, nullptr);
}

KeyColour MappedKey::getKeyColour() const {
    return (KeyColour)valueTree[id_keyColour].toString().getIntValue();
}

void MappedKey::setKeyColour(KeyColour colour) {
    valueTree.setProperty(id_keyColour, (int)colour, nullptr);
}

Zone MappedKey::getZone() const {
    return (Zone)valueTree[id_zone].toString().getIntValue();
}

void MappedKey::setZone(Zone zone) {
    valueTree.setProperty(id_zone, zone, nullptr);
}

KeyMappingType MappedKey::getMappingType() const {
    return (KeyMappingType)valueTree[id_keyMappingType].toString().getIntValue();
}

void MappedKey::setMappingType(KeyMappingType mappingType) {
    valueTree.setProperty(id_keyMappingType, mappingType, nullptr);
    mappingType == KeyMappingType::Note ?
        setMappingValue("24") :
    setMappingValue("");
}

juce::String MappedKey::getMappingValue() const {
    return valueTree[id_mappingValue].toString();
}

void MappedKey::setMappingValue(juce::String mappingValue) {
    valueTree.setProperty(id_mappingValue, mappingValue, nullptr);
}

juce::String MappedKey::getKeyId() const {
    return valueTree[id_keyId].toString();
}

void MappedKey::setKeyId(juce::String keyId) {
    valueTree.setProperty(id_keyId, keyId, nullptr);
}


