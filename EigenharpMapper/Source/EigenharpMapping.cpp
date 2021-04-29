#include "EigenharpMapping.h"

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
            keyRowLengths[4] = 0;
            buttonCount = 8;
            stripCount = 1;
            break;
        case InstrumentType::Pico:
            normalKeyCount = 18;
            percKeyCount = 0;
            keyRowCount = 2;
            keyRowLengths[0] = 9;
            keyRowLengths[1] = 9;
            keyRowLengths[2] = 0;
            keyRowLengths[3] = 0;
            keyRowLengths[4] = 0;
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

}

EigenharpMapping::~EigenharpMapping() {
}

void EigenharpMapping::addMappedKey(EigenharpKeyType keyType) {
    auto key = MappedKey(keyType, valueTree);
    mappedKeys.push_back(key);
    key.setKeyId(juce::String(mappedKeys.size()));
}

juce::ValueTree EigenharpMapping::getValueTree() const {
    return valueTree;
}

void EigenharpMapping::setValueTree(juce::ValueTree valueTree) {
    this->valueTree = valueTree;
    for (int i = 0; i < valueTree.getNumChildren(); i++) {
        mappedKeys[i].setValueTree(valueTree.getChild(i));
    }
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
