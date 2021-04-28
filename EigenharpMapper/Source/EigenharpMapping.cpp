#include "EigenharpMapping.h"

MappedKey::MappedKey(EigenharpKeyType keyType, juce::ValueTree &rootValueTree): valueTree("key") {
    setKeyColour(KeyColour::Off);
    setKeyType(keyType);
    setZone(Zone::NoZone);
    keyType == EigenharpKeyType::Button ?
        setMappingType(KeyMappingType::None) :
        setMappingType(KeyMappingType::Note);

    rootValueTree.addChild(valueTree, -1, nullptr);
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



EigenharpMapping::EigenharpMapping(InstrumentType instrumentType): valueTree("layout") {
    switch(instrumentType) {
        case InstrumentType::Alpha:
            normalKeyCount = 120;
            percKeyCount = 12;
            keyRowCount = 5;
            keyRowLengths[0] = 24;
            keyRowLengths[1] = 24;
            keyRowLengths[2] = 24;
            keyRowLengths[3] = 24;
            keyRowLengths[4] = 24;
            buttonCount = 0;
            stripCount = 2;
            break;
        case InstrumentType::Tau:
            normalKeyCount = 72;
            percKeyCount = 12;
            keyRowCount = 4;
            keyRowLengths[0] = 16;
            keyRowLengths[1] = 16;
            keyRowLengths[2] = 20;
            keyRowLengths[3] = 20;
            buttonCount = 8;
            stripCount = 1;
            break;
        case InstrumentType::Pico:
            normalKeyCount = 18;
            percKeyCount = 0;
            keyRowCount = 2;
            keyRowLengths[0] = 9;
            keyRowLengths[1] = 9;
            buttonCount = 4;
            stripCount = 1;
            break;
        default:
            break;
    }
    

    valueTree.setProperty(id_instrumentType, instrumentType, nullptr);
    for (int i = 0; i < normalKeyCount; i++)
        addMappedKey(EigenharpKeyType::Normal);
    for (int i = getPercKeyStartIndex(); i < getButtonStartIndex(); i++)
        addMappedKey(EigenharpKeyType::Perc);
    for (int i = getButtonStartIndex(); i < getTotalKeyCount(); i++)
        addMappedKey(EigenharpKeyType::Button);

//    logXML();
}

EigenharpMapping::~EigenharpMapping() {
}

void EigenharpMapping::addMappedKey(EigenharpKeyType keyType) {
    auto key = MappedKey(keyType, valueTree);
    mappedKeys.push_back(key);
    key.setKeyId(juce::String(mappedKeys.size()));
}

void EigenharpMapping::logXML() {
    std::cout << valueTree.toXmlString();
}

int EigenharpMapping::getNormalkeyCount() const {
    return normalKeyCount;
}

int EigenharpMapping::getPercKeyCount() const {
    return percKeyCount;
}

int EigenharpMapping::getButtonCount() const {
    return buttonCount;
}

int EigenharpMapping::getStripCount() const {
    return stripCount;
}

int EigenharpMapping::getKeyRowCount() const {
    return keyRowCount;
}

const int* EigenharpMapping::getKeyRowLengths() const {
    return keyRowLengths;
}

int EigenharpMapping::getTotalKeyCount() const {
    return normalKeyCount + percKeyCount + buttonCount;
}

int EigenharpMapping::getPercKeyStartIndex() const {
    return normalKeyCount;
}

int EigenharpMapping::getButtonStartIndex() const {
    return normalKeyCount + percKeyCount;
}

InstrumentType EigenharpMapping::getInstrumentType() const {
    return (InstrumentType)valueTree[id_instrumentType].toString().getIntValue();
}

MappedKey* EigenharpMapping::getMappedKeys() {
    return mappedKeys.data();
}
