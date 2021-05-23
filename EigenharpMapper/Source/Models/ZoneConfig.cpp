#include "ZoneConfig.h"

ZoneConfig::ZoneConfig(int tabNo, Zone zone, juce::ValueTree parentTree) {
    valueTree = parentTree.getOrCreateChildWithName("zone" + juce::String(tabNo) + "_" + juce::String((int)zone), nullptr);
    this->zone = zone;
    if (!valueTree.getChildWithName(id_pressure).isValid()) {
        juce::ValueTree midiPressureTree(id_pressure);
        valueTree.appendChild(midiPressureTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_roll).isValid()) {
        juce::ValueTree midiRollTree(id_roll);
        valueTree.appendChild(midiRollTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_yaw).isValid()) {
        juce::ValueTree midiYawTree(id_yaw);
        valueTree.appendChild(midiYawTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_strip1Rel).isValid()) {
        juce::ValueTree midiStrip1RelTree(id_strip1Rel);
        valueTree.appendChild(midiStrip1RelTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_strip1Abs).isValid()) {
        juce::ValueTree midiStrip1AbsTree(id_strip1Abs);
        valueTree.appendChild(midiStrip1AbsTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_strip2Rel).isValid()) {
        juce::ValueTree midiStrip2RelTree(id_strip2Rel);
        valueTree.appendChild(midiStrip2RelTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_strip2Abs).isValid()) {
        juce::ValueTree midiStrip2AbsTree(id_strip2Abs);
        valueTree.appendChild(midiStrip2AbsTree, nullptr);
    }
    if (!valueTree.getChildWithName(id_breath).isValid()) {
        juce::ValueTree midiBreathTree(id_breath);
        valueTree.appendChild(midiBreathTree, nullptr);
    }
}

juce::ValueTree ZoneConfig::getValueTree() const {
    return valueTree;
}

bool ZoneConfig::isEnabled() const {
    return valueTree.getProperty(id_enabled).toString().getIntValue() > 0;
}

void ZoneConfig::setEnabled(bool enabled) {
    valueTree.setProperty(id_enabled, enabled, nullptr);
}

int ZoneConfig::getTranspose() const {
    bool hasValue = !valueTree.getProperty(id_transpose).isVoid();
    return hasValue ? valueTree.getProperty(id_transpose).toString().getIntValue() : INT_MIN;
}

void ZoneConfig::setTranspose(int transpose) {
    valueTree.setProperty(id_transpose, transpose, nullptr);
}

int ZoneConfig::getGlobalPitchbend() const {
    bool hasValue = !valueTree.getProperty(id_globalPitchbend).isVoid();
    return hasValue ? valueTree.getProperty(id_globalPitchbend).toString().getIntValue() : INT_MIN;
}

void ZoneConfig::setGlobalPitchbend(int pitchbend) {
    valueTree.setProperty(id_globalPitchbend, pitchbend, nullptr);
}

int ZoneConfig::getKeyPitchbend() const {
    bool hasValue = !valueTree.getProperty(id_keyPitchbend).isVoid();
    return hasValue ? valueTree.getProperty(id_keyPitchbend).toString().getIntValue() : INT_MIN;
}

void ZoneConfig::setKeyPitchbend(int pitchbend) {
    valueTree.setProperty(id_keyPitchbend, pitchbend, nullptr);
}

MidiChannelType ZoneConfig::getMidiChannelType() const {
    return (MidiChannelType)valueTree[id_midiChannelType].toString().getIntValue();
}

void ZoneConfig::setMidiChannelType(MidiChannelType type) {
    valueTree.setProperty(id_midiChannelType, (int)type, nullptr);
}

void ZoneConfig::addListener(juce::ValueTree::Listener *listener) {
    valueTree.addListener(listener);
}

ZoneConfig::MidiValue ZoneConfig::getMidiValue(juce::Identifier childId) const {
    auto midiValChild = valueTree.getChildWithName(childId);
    return MidiValue {
        .valueType = (MidiValueType)midiValChild.getProperty(id_midiValType).toString().getIntValue(),
        .ccNo = midiValChild.getProperty(id_midiCCNo)
    };
}

void ZoneConfig::setMidiValue(juce::Identifier childId, MidiValue midiValue) {
    auto midiValChild = valueTree.getChildWithName(childId);
    midiValChild.setProperty(id_midiValType, (int)midiValue.valueType, nullptr);
    midiValChild.setProperty(id_midiCCNo, midiValue.ccNo, nullptr);
}
