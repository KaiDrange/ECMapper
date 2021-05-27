#include "KeyConfigLookup.h"

KeyConfigLookup::KeyConfigLookup(DeviceType deviceType) {
    this->deviceType = deviceType;
//    updateAll();
}

void KeyConfigLookup::updateAll() {
    juce::ValueTree layoutTree = LayoutWrapper::getLayoutTree(deviceType);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        if (!layoutTree.getChild(i).getType().toString().startsWith(LayoutWrapper::id_key + "_"))
            continue;
        
        LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(layoutTree.getChild(i));
        Key key;
        key.mapType = layoutKey.keyMappingType;
        key.note = key.mapType == KeyMappingType::Note ? layoutKey.mappingValue.getIntValue() : 0;
        keys[layoutKey.keyId.course][layoutKey.keyId.keyNo] = key;
    }
}
