#include "ZoneConfig.h"

ZoneConfig::ZoneConfig(Zone zone): valueTree("zone") {
    this->zone = zone;
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
