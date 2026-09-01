#include "ZoneWrapper.h"

namespace ecm {

void ZoneWrapper::addListener(InstrumentType deviceType, juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    auto vTree1 = getZoneTree(deviceType, Zone::Zone1, rootState);
    vTree1.addListener(listener);
    auto vTree2 = getZoneTree(deviceType, Zone::Zone2, rootState);
    vTree2.addListener(listener);
    auto vTree3 = getZoneTree(deviceType, Zone::Zone3, rootState);
    vTree3.addListener(listener);
}

void ZoneWrapper::removeListener(InstrumentType deviceType, juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    auto vTree1 = getZoneTree(deviceType, Zone::Zone1, rootState);
    vTree1.removeListener(listener);
    auto vTree2 = getZoneTree(deviceType, Zone::Zone2, rootState);
    vTree2.removeListener(listener);
    auto vTree3 = getZoneTree(deviceType, Zone::Zone3, rootState);
    vTree3.removeListener(listener);
}

juce::ValueTree ZoneWrapper::getZoneTree(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState) {
    auto deviceChild = rootState.getOrCreateChildWithName(id_device + juce::String((int)deviceType), nullptr);
    return deviceChild.getOrCreateChildWithName(id_zone + juce::String((int)zone), nullptr);
}

MidiChannelType ZoneWrapper::getMidiChannelType(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return default_midiChannelType;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    return (MidiChannelType)int(zoneTree.getProperty(id_midiChannelType, (int)default_midiChannelType));
}

void ZoneWrapper::setMidiChannelType(InstrumentType deviceType, Zone zone, MidiChannelType midiChannelType, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    zoneTree.setProperty(id_midiChannelType, (int)midiChannelType, nullptr);
}

int ZoneWrapper::getTranspose(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return default_transpose;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    return zoneTree.getProperty(id_transpose, default_transpose);
}

void ZoneWrapper::setTranspose(InstrumentType deviceType, Zone zone, int value, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    zoneTree.setProperty(id_transpose, value, nullptr);
}

juce::String ZoneWrapper::getTransposeParameterID(InstrumentType deviceType, Zone zone) {
    return "transpose_" + juce::String((int)deviceType) + "_" + juce::String((int)zone);
}

juce::String ZoneWrapper::getEnabledParameterID(InstrumentType deviceType, Zone zone) {
    return "enabled_" + juce::String((int)deviceType) + "_" + juce::String((int)zone);
}

void ZoneWrapper::setKeyPitchbend(InstrumentType deviceType, Zone zone, int value, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    zoneTree.setProperty(id_keyPitchbend, value, nullptr);
}

int ZoneWrapper::getKeyPitchbend(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return default_keyPitchbend;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    return zoneTree.getProperty(id_keyPitchbend, default_keyPitchbend);
}

void ZoneWrapper::setChannelMaxPitchbend(InstrumentType deviceType, Zone zone, int value, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    zoneTree.setProperty(id_channelMaxPitchbend, value, nullptr);
}

int ZoneWrapper::getChannelMaxPitchbend(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return default_channelMaxPitchbend;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    return zoneTree.getProperty(id_channelMaxPitchbend, default_channelMaxPitchbend);
}

void ZoneWrapper::setEnabled(InstrumentType deviceType, Zone zone, bool enabled, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    zoneTree.setProperty(id_enabled, enabled, nullptr);
}

bool ZoneWrapper::getEnabled(InstrumentType deviceType, Zone zone, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return default_enabled;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    return zoneTree.getProperty(id_enabled, default_enabled);
}

ZoneWrapper::MidiValue ZoneWrapper::getMidiValue(InstrumentType deviceType, Zone zone, juce::Identifier childId, MidiValue defaultValue, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return defaultValue;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    auto midiValChild = zoneTree.getChildWithName(childId);
    if (midiValChild.isValid()) {
        return MidiValue {
            .valueType = (MidiValueType)int(midiValChild.getProperty(id_midiValType)),
            .ccNo = (int)midiValChild.getProperty(id_midiCCNo)
        };
    }
    return defaultValue;
}

void ZoneWrapper::setMidiValue(InstrumentType deviceType, Zone zone, juce::Identifier childId, MidiValue midiValue, juce::ValueTree& rootState) {
    if (deviceType == InstrumentType::None) return;
    auto zoneTree = getZoneTree(deviceType, zone, rootState);
    auto midiValChild = zoneTree.getOrCreateChildWithName(childId, nullptr);
    midiValChild.setProperty(id_midiValType, (int)midiValue.valueType, nullptr);
    midiValChild.setProperty(id_midiCCNo, midiValue.ccNo, nullptr);
}

InstrumentType ZoneWrapper::getInstrumentTypeFromTree(juce::ValueTree tree) {
    auto parentTree = tree.getParent();
    while (parentTree.isValid() && !parentTree.getType().toString().startsWith(id_device.toString()))
        parentTree = parentTree.getParent();
    
    if (parentTree.isValid())
        return (InstrumentType)parentTree.getType().toString().substring(6).getIntValue();
    
    return InstrumentType::None;
}

} // namespace ecm
