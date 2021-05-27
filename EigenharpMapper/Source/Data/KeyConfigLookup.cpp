#include "KeyConfigLookup.h"

KeyConfigLookup::KeyConfigLookup(DeviceType deviceType) {
    this->deviceType = deviceType;
}

void KeyConfigLookup::updateAll() {
    juce::ValueTree layoutTree = LayoutWrapper::getLayoutTree(deviceType);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        updateKey(layoutTree.getChild(i));
    }
}

void KeyConfigLookup::updateKey(juce::ValueTree keytree) {
    if (!keytree.getType().toString().startsWith(LayoutWrapper::id_key + "_"))
        return;
    
    LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(keytree);
    if (layoutKey.keyId.deviceType == DeviceType::None)
        return;

    bool setKeyToDefault = false;
    if (layoutKey.keyMappingType == KeyMappingType::None)
        setKeyToDefault = true;
    if (layoutKey.zone == Zone::NoZone)
        setKeyToDefault = true;
    if (!ZoneWrapper::getEnabled(layoutKey.keyId.deviceType, layoutKey.zone))
        setKeyToDefault = true;
    
    Key key;
    if (!setKeyToDefault) {
        key.mapType = layoutKey.keyMappingType;
        key.note = key.mapType == KeyMappingType::Note
            ? std::min(std::max(layoutKey.mappingValue.getIntValue() + ZoneWrapper::getTranspose(layoutKey.keyId.deviceType, layoutKey.zone), 0), 127)
            : 0;
        key.pressure = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_pressure, ZoneWrapper::default_pressure);
        key.roll = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_roll, ZoneWrapper::default_roll);
        key.yaw = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_yaw, ZoneWrapper::default_yaw);
        key.output = ZoneWrapper::getMidiChannelType(layoutKey.keyId.deviceType, layoutKey.zone);
        auto keyPB = ZoneWrapper::getKeyPitchbend(layoutKey.keyId.deviceType, layoutKey.zone);
        if (key.output == MidiChannelType::MPE_Low)
            key.pbRange = std::min(((float)keyPB)/((float)SettingsWrapper::getLowMPEPB()), 1.0f);
        else if (key.output == MidiChannelType::MPE_High)
            key.pbRange = std::min(((float)keyPB)/((float)SettingsWrapper::getHighMPEPB()), 1.0f);
        else //TODO: Add a non-mpe max pb
            key.pbRange = std::min(((float)keyPB)/12.0f, 1.0f);
    }
    keys[layoutKey.keyId.course][layoutKey.keyId.keyNo] = key;
}
