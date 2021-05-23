#include "KeyConfigLookup.h"

KeyConfigLookup::KeyConfigLookup(juce::ValueTree layoutTree) {
    this->layoutTree = layoutTree;
    updateAll();
}

void KeyConfigLookup::updateAll() {
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        auto keyChild = layoutTree.getChild(i);
        Key key;
        key.mapType = (KeyMappingType)keyChild.getProperty("keyMappingType").toString().getIntValue();
        key.note = key.mapType == KeyMappingType::Note ? keyChild.getProperty("mappingValue").toString().getIntValue() : 0;
        keys[0][i] = key;
    }

//    if (layout->getDeviceType() == DeviceType::Pico) {
//        //TODO: rest of courses
//    }
}
