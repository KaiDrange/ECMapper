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
    auto zoneTree = getZoneTree(deviceType, zone);
    return (MidiChannelType)int(zoneTree.getProperty(id_midiChannelType, (int)default_midiChannelType));
}

void ZoneWrapper::setMidiChannelType(DeviceType deviceType, Zone zone, MidiChannelType midiChannelType) {
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_midiChannelType, (int)midiChannelType, nullptr);
}

int ZoneWrapper::getTranspose(DeviceType deviceType, Zone zone) {
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_transpose, default_transpose);
}

void ZoneWrapper::setTranspose(DeviceType deviceType, Zone zone, int value) {
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_transpose, value, nullptr);
}

void ZoneWrapper::setKeyPitchbend(DeviceType deviceType, Zone zone, int value) {
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_keyPitchbend, value, nullptr);
}

int ZoneWrapper::getKeyPitchbend(DeviceType deviceType, Zone zone) {
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_keyPitchbend, default_keyPitchbend);
}

void ZoneWrapper::setEnabled(DeviceType deviceType, Zone zone, bool enabled) {
    auto zoneTree = getZoneTree(deviceType, zone);
    zoneTree.setProperty(id_enabled, enabled, nullptr);
}

bool ZoneWrapper::getEnabled(DeviceType deviceType, Zone zone) {
    auto zoneTree = getZoneTree(deviceType, zone);
    return zoneTree.getProperty(id_enabled, default_enabled);
}

ZoneWrapper::MidiValue ZoneWrapper::getMidiValue(DeviceType deviceType, Zone zone, juce::Identifier childId, ZoneWrapper::MidiValue defaultValue) {
    auto zoneTree = getZoneTree(deviceType, zone);
    auto midiValChild = zoneTree.getChildWithName(childId);
    return midiValChild.isValid() ?
    ZoneWrapper::MidiValue {
            .valueType = (MidiValueType)int(midiValChild.getProperty(id_midiValType)),
            .ccNo = midiValChild.getProperty(id_midiCCNo)
        } : defaultValue;
}

void ZoneWrapper::setMidiValue(DeviceType deviceType, Zone zone, juce::Identifier childId, MidiValue midiValue) {
    auto zoneTree = getZoneTree(deviceType, zone);
    auto midiValChild = zoneTree.getOrCreateChildWithName(childId, nullptr);
    midiValChild.setProperty(id_midiValType, (int)midiValue.valueType, nullptr);
    midiValChild.setProperty(id_midiCCNo, midiValue.ccNo, nullptr);
}
