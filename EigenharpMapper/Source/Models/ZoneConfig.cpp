#include "ZoneConfig.h"

ZoneConfig::ZoneConfig(Zone zone): valueTree("zone") {
    this->zone = zone;
    juce::ValueTree midiPressureTree(id_pressure);
    valueTree.addChild(midiPressureTree, 0, nullptr);
    juce::ValueTree midiRollTree(id_roll);
    valueTree.addChild(midiRollTree, 1, nullptr);
    juce::ValueTree midiYawTree(id_yaw);
    valueTree.addChild(midiYawTree, 2, nullptr);
    juce::ValueTree midiStrip1RelTree(id_strip1Rel);
    valueTree.addChild(midiStrip1RelTree, 3, nullptr);
    juce::ValueTree midiStrip1AbsTree(id_strip1Abs);
    valueTree.addChild(midiStrip1AbsTree, 4, nullptr);
    juce::ValueTree midiStrip2RelTree(id_strip2Rel);
    valueTree.addChild(midiStrip2RelTree, 5, nullptr);
    juce::ValueTree midiStrip2AbsTree(id_strip2Abs);
    valueTree.addChild(midiStrip2AbsTree, 6, nullptr);
    juce::ValueTree midiBreathTree(id_breath);
    valueTree.addChild(midiBreathTree, 7, nullptr);
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
    return valueTree.getProperty(id_transpose).toString().getIntValue();
}

void ZoneConfig::setTranspose(int transpose) {
    valueTree.setProperty(id_transpose, transpose, nullptr);
}

int ZoneConfig::getGlobalPitchbend() const {
    return valueTree.getProperty(id_globalPitchbend).toString().getIntValue();
}

void ZoneConfig::setGlobalPitchbend(int pitchbend) {
    valueTree.setProperty(id_globalPitchbend, pitchbend, nullptr);
}

int ZoneConfig::getKeyPitchbend() const {
    return valueTree.getProperty(id_keyPitchbend).toString().getIntValue();
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

//juce::String ZoneConfig::getPressure() const {
//    return valueTree.getProperty(id_pressure).toString();
//}
//void ZoneConfig::setPressure(juce::String pressure) {
//    valueTree.setProperty(id_pressure, pressure, nullptr);
//}
//
//juce::String ZoneConfig::getRoll() const {
//    return valueTree.getProperty(id_roll).toString();
//}
//
//void ZoneConfig::setRoll(juce::String roll) {
//    valueTree.setProperty(id_pressure, roll, nullptr);
//}
//
//juce::String ZoneConfig::getYaw() const {
//    return valueTree.getProperty(id_yaw).toString();
//}
//
//void ZoneConfig::setYaw(juce::String yaw) {
//    valueTree.setProperty(id_roll, yaw, nullptr);
//}
//
//juce::String ZoneConfig::getStrip1Rel() const {
//    return valueTree.getProperty(id_strip1Rel).toString();
//}
//
//void ZoneConfig::setStrip1Rel(juce::String strip1rel) {
//    valueTree.setProperty(id_strip1Rel, strip1rel, nullptr);
//}
//
//juce::String ZoneConfig::getStrip1Abs() const {
//    return valueTree.getProperty(id_strip1Abs).toString();
//}
//
//void ZoneConfig::setStrip1Abs(juce::String strip1abs) {
//    valueTree.setProperty(id_strip1Abs, strip1abs, nullptr);
//}
//
//juce::String ZoneConfig::getStrip2Rel() const {
//    return valueTree.getProperty(id_strip2Rel).toString();
//}
//
//void ZoneConfig::setStrip2Rel(juce::String strip2rel) {
//    valueTree.setProperty(id_strip2Rel, strip2rel, nullptr);
//}
//
//juce::String ZoneConfig::getStrip2Abs() const {
//    return valueTree.getProperty(id_strip2Abs).toString();
//}
//
//void ZoneConfig::setStrip2Abs(juce::String strip2abs) {
//    valueTree.setProperty(id_strip2Abs, strip2abs, nullptr);
//}
//
//juce::String ZoneConfig::getBreath() const {
//    return valueTree.getProperty(id_breath).toString();
//}
//
//void ZoneConfig::setBreath(juce::String breath) {
//    valueTree.setProperty(id_breath, breath, nullptr);
//}

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
