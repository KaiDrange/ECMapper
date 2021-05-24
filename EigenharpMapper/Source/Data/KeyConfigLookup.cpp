#include "KeyConfigLookup.h"

KeyConfigLookup::KeyConfigLookup(juce::ValueTree layoutTree, juce::ValueTree zoneTree1, juce::ValueTree zoneTree2, juce::ValueTree zoneTree3) {
    this->layoutTree = layoutTree;
    this->zoneTrees[0] = zoneTree1;
    this->zoneTrees[1] = zoneTree2;
    this->zoneTrees[2] = zoneTree3;
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
