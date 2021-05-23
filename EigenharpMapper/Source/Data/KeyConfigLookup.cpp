#include "KeyConfigLookup.h"

KeyConfigLookup::KeyConfigLookup(juce::ValueTree layoutTree) {
    this->layoutTree = layoutTree;
    updateAll();
}

void KeyConfigLookup::updateAll() {
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        KeyConfig keyConfig(layoutTree.getChild(i));
        Key key;
        key.mapType = keyConfig.getMappingType();
        key.note = key.mapType == KeyMappingType::Note ? keyConfig.getMappingValue().getIntValue() : 0;
        keys[0][i] = key;
    }

//    if (layout->getDeviceType() == DeviceType::Pico) {
//        //TODO: rest of courses
//    }
}
