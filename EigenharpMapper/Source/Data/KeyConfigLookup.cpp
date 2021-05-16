#include "KeyConfigLookup.h"

void KeyConfigLookup::setActiveLayout(Layout *layout) {
    this->layout = layout;
    updateAll();
}

void KeyConfigLookup::updateAll() {
    KeyConfig *keyConfigs = layout->getKeyConfigs();
    for (int i = 0; i < layout->getNormalkeyCount(); i++) {
        Key key;
        key.mapType = keyConfigs[i].getMappingType();
        key.note = key.mapType == KeyMappingType::Note ? keyConfigs[i].getMappingValue().getIntValue() : 0;
        keys[0][i] = key;
    }
        
    if (layout->getInstrumentType() == InstrumentType::Pico) {
        //TODO: rest of courses
    }
}