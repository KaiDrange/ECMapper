#include "KeyConfigLookup.h"

KeyConfigLookup::KeyConfigLookup(DeviceType deviceType) {
    this->deviceType = deviceType;
//    this->layoutTree = layoutTree;
//    this->zoneTrees[0] = zoneTree1;
//    this->zoneTrees[1] = zoneTree2;
//    this->zoneTrees[2] = zoneTree3;
    updateAll();
}

void KeyConfigLookup::updateAll() {
    juce::ValueTree layoutTree = LayoutWrapper::getLayoutTree(deviceType);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        LayoutWrapper::KeyId keyId = {
            .deviceType = deviceType,
            .course = 0,
            .keyNo = i
        };
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKey(keyId);
        Key key;
        key.mapType = layoutKey.keyMappingType;
        key.note = key.mapType == KeyMappingType::Note ? layoutKey.mappingValue.getIntValue() : 0;
        keys[0][i] = key;
    }

//    if (layout->getDeviceType() == DeviceType::Pico) {
//        //TODO: rest of courses
//    }
}
