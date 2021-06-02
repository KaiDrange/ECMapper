#include "ZoneWrapper.h"

void ZoneWrapper::addListener(DeviceType deviceType, juce::ValueTree::Listener *listener) {
    auto vTree = getZoneTree(deviceType, Zone::Zone1);
    vTree.addListener(listener);
    vTree = getZoneTree(deviceType, Zone::Zone2);
    vTree.addListener(listener);
    vTree = getZoneTree(deviceType, Zone::Zone3);
    vTree.addListener(listener);
}

juce::ValueTree ZoneWrapper::getZoneTree(DeviceType deviceType, Zone zone) {
    auto deviceChild = rootState->getOrCreateChildWithName(id_device + juce::String((int)deviceType), nullptr);
    return deviceChild.getOrCreateChildWithName(id_zone + juce::String((int)zone), nullptr);
}

MidiChannelType ZoneWrapper::getMidiChannelType(DeviceType deviceType, Zone zone) {
    if (deviceType == DeviceType::None) return default_midiChannelType;
    auto zoneTree = getZoneTree(deviceType, zone);
    return (MidiChannelType)int(zoneTree.getProperty(id_midiChannelType, (int)default_midiChannelType));
}

void ZoneWrapper::setMidiChannelType(DeviceType deviceType, Zone zone, MidiChannelType midiChannelType) {
    if (deviceType == DeviceType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_midiChannelType, (int)midiChannelType, nullptr);
}

int ZoneWrapper::getTranspose(DeviceType deviceType, Zone zone) {
    if (deviceType == DeviceType::None) return default_transpose;
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_transpose, default_transpose);
}

void ZoneWrapper::setTranspose(DeviceType deviceType, Zone zone, int value) {
    if (deviceType == DeviceType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_transpose, value, nullptr);
}

void ZoneWrapper::setKeyPitchbend(DeviceType deviceType, Zone zone, int value) {
    if (deviceType == DeviceType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_keyPitchbend, value, nullptr);
}

int ZoneWrapper::getKeyPitchbend(DeviceType deviceType, Zone zone) {
    if (deviceType == DeviceType::None) return default_keyPitchbend;
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_keyPitchbend, default_keyPitchbend);
}

void ZoneWrapper::setChannelMaxPitchbend(DeviceType deviceType, Zone zone, int value) {
    if (deviceType == DeviceType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_channelMaxPitchbend, value, nullptr);
}

int ZoneWrapper::getChannelMaxPitchbend(DeviceType deviceType, Zone zone) {
    if (deviceType == DeviceType::None) return default_channelMaxPitchbend;
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_channelMaxPitchbend, default_channelMaxPitchbend);
}

void ZoneWrapper::setEnabled(DeviceType deviceType, Zone zone, bool enabled) {
    if (deviceType == DeviceType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_enabled, enabled, nullptr);
}

bool ZoneWrapper::getEnabled(DeviceType deviceType, Zone zone) {
    if (deviceType == DeviceType::None) return default_enabled;
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_enabled, default_enabled);
}

ZoneWrapper::MidiValue ZoneWrapper::getMidiValue(DeviceType deviceType, Zone zone, juce::Identifier childId, ZoneWrapper::MidiValue defaultValue) {
    if (deviceType == DeviceType::None) return defaultValue;
    auto zoneTree = getZoneTree(deviceType, zone);
    auto midiValChild = zoneTree.getChildWithName(childId);
    return midiValChild.isValid() ?
    ZoneWrapper::MidiValue {
            .valueType = (MidiValueType)int(midiValChild.getProperty(id_midiValType)),
            .ccNo = midiValChild.getProperty(id_midiCCNo)
        } : defaultValue;
}

void ZoneWrapper::setMidiValue(DeviceType deviceType, Zone zone, juce::Identifier childId, MidiValue midiValue) {
    if (deviceType == DeviceType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone);
    auto midiValChild = zoneTree.getOrCreateChildWithName(childId, nullptr);
    midiValChild.setProperty(id_midiValType, (int)midiValue.valueType, nullptr);
    midiValChild.setProperty(id_midiCCNo, midiValue.ccNo, nullptr);
}

DeviceType ZoneWrapper::getDeviceTypeFromTree(juce::ValueTree tree) {
    auto parentTree = tree.getParent();
    while (!parentTree.getType().toString().startsWith(id_device))
        parentTree = parentTree.getParent();
    
    return (DeviceType)parentTree.getType().toString().substring(6).getIntValue();
}
