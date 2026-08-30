#include "ConfigLookup.h"

namespace ecm {

ConfigLookup::ConfigLookup(InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState)
    : pluginState(pluginState), deviceType(deviceType) {
}

void ConfigLookup::updateAll() {
    const juce::ScopedLock sl(lock_);
    this->controlLights = SettingsWrapper::getControlLights(deviceType, pluginState.state);

    for (int course = 0; course < 3; ++course) {
        for (int keyNo = 0; keyNo < 120; ++keyNo) {
            updateKey({course, keyNo, deviceType});
        }
    }
    updateBreath(Zone::Zone1);
    updateBreath(Zone::Zone2);
    updateBreath(Zone::Zone3);
    
    updateStrips(Zone::Zone1);
    updateStrips(Zone::Zone2);
    updateStrips(Zone::Zone3);
}

void ConfigLookup::updateKey(juce::ValueTree keytree) {
    if (!keytree.getType().toString().startsWith(LayoutWrapper::id_key.toString() + "_"))
        return;
    
    LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKeyFromKeyTree(keytree);
    if (layoutKey.keyId.deviceType == InstrumentType::None)
        return;

    updateKey(layoutKey.keyId);
}

void ConfigLookup::updateKey(LayoutWrapper::KeyId keyId) {
    const juce::ScopedLock sl(lock_);
    LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKey(keyId, pluginState.state);
    
    bool setKeyToDefault = false;
    if (layoutKey.keyMappingType == KeyMappingType::None)
        setKeyToDefault = true;
    if (layoutKey.zone == Zone::NoZone)
        setKeyToDefault = true;
    if (!ZoneWrapper::getEnabled(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state))
        setKeyToDefault = true;
    
    Key key;
    key.keyId = layoutKey.keyId;

    if (!setKeyToDefault) {
        key.keyType = layoutKey.keyType;
        key.mapType = layoutKey.keyMappingType;
        key.keyColour = layoutKey.keyColour;
        for (int i = 0; i < 4; i++)
            key.notes[i] = -1;
            
        if (key.mapType == KeyMappingType::Chord) {
            juce::StringArray chordParts;
            Utils::splitString(layoutKey.mappingValue, ";", chordParts);
            if (chordParts.size() == 5) {
                for (int i = 0; i < 4; i++) {
                    int noteNumber = chordParts[i+1].getIntValue();
                    key.notes[i] = noteNumber < 0
                       ? -1
                       : std::clamp(noteNumber + ZoneWrapper::getTranspose(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state), 0, 127);
                }
            }
        }
        else {
            key.notes[0] = key.mapType == KeyMappingType::Note
                ? std::clamp(layoutKey.mappingValue.getIntValue() + ZoneWrapper::getTranspose(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state), 0, 127)
                : -1;
        }
        
        key.pressure = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_pressure, ZoneWrapper::default_pressure, pluginState.state);
        key.roll = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_roll, ZoneWrapper::default_roll, pluginState.state);
        key.yaw = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_yaw, ZoneWrapper::default_yaw, pluginState.state);
        key.output = ZoneWrapper::getMidiChannelType(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state);
        
        auto keyPB = ZoneWrapper::getKeyPitchbend(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state);
        auto getSafePbRange = [](float pb, float maxPb) {
            return maxPb > 0.0f ? std::min(pb / maxPb, 1.0f) : 0.0f;
        };

        if (key.output == MidiChannelType::MPE_Low)
            key.pbRange = getSafePbRange((float)keyPB, (float)SettingsWrapper::getLowerMPEPB(pluginState.state));
        else if (key.output == MidiChannelType::MPE_High)
            key.pbRange = getSafePbRange((float)keyPB, (float)SettingsWrapper::getUpperMPEPB(pluginState.state));
        else
            key.pbRange = getSafePbRange((float)keyPB, (float)ZoneWrapper::getChannelMaxPitchbend(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state));
        
        if (key.mapType != KeyMappingType::MidiMsg) {
            key.cmdType = 0;
            key.msgType = 0;
            key.cmdCC = 0;
            key.cmdOn = 0;
            key.cmdOff = 0;
        }
        else {
            juce::StringArray cmdParts;
            Utils::splitString(layoutKey.mappingValue, ";", cmdParts);
            if (cmdParts.size() == 5) {
                if (cmdParts[0] == "Latch") key.cmdType = 1;
                else if (cmdParts[0] == "Momentary") key.cmdType = 2;
                else if (cmdParts[0] == "Trigger") key.cmdType = 3;
                else key.cmdType = 0;

                if (cmdParts[1] == "CC") key.msgType = 1;
                else if (cmdParts[1] == "PC") key.msgType = 2;
                else if (cmdParts[1] == "Realtime") key.msgType = 3;
                else if (cmdParts[1] == "AllNotesOff") key.msgType = 4;
                else key.msgType = 0;
                
                key.cmdCC = cmdParts[2].getIntValue();
                key.cmdOff = cmdParts[3].getIntValue();
                key.cmdOn = cmdParts[4].getIntValue();
            }
        }
    }
    if (layoutKey.keyId.course < 3 && layoutKey.keyId.keyNo < 120)
        keys[layoutKey.keyId.course][layoutKey.keyId.keyNo] = key;
}

void ConfigLookup::updateBreath(Zone zone) {
    const juce::ScopedLock sl(lock_);
    int zoneIdx = (int)zone - 1;
    if (zoneIdx < 0 || zoneIdx > 2) return;

    if (!ZoneWrapper::getEnabled(deviceType, zone, pluginState.state)) {
        breath[zoneIdx].channel = 0;
        breath[zoneIdx].midiValue.valueType = MidiValueType::Off;
        return;
    }
    
    auto midiChannelType = ZoneWrapper::getMidiChannelType(deviceType, zone, pluginState.state);
    if (midiChannelType == MidiChannelType::MPE_Low)
        breath[zoneIdx].channel = 1;
    else if (midiChannelType == MidiChannelType::MPE_High)
        breath[zoneIdx].channel = 16;
    else
        breath[zoneIdx].channel = (int)midiChannelType;
    
    breath[zoneIdx].midiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_breath, ZoneWrapper::default_breath, pluginState.state);
}

void ConfigLookup::updateStrips(Zone zone) {
    const juce::ScopedLock sl(lock_);
    int zoneIdx = (int)zone - 1;
    if (zoneIdx < 0 || zoneIdx > 2) return;

    if (!ZoneWrapper::getEnabled(deviceType, zone, pluginState.state)) {
        strip1[zoneIdx].channel = 0;
        strip2[zoneIdx].channel = 0;
        strip1[zoneIdx].absMidiValue.valueType = MidiValueType::Off;
        strip1[zoneIdx].relMidiValue.valueType = MidiValueType::Off;
        strip2[zoneIdx].absMidiValue.valueType = MidiValueType::Off;
        strip2[zoneIdx].relMidiValue.valueType = MidiValueType::Off;
        return;
    }
    
    auto midiChannelType = ZoneWrapper::getMidiChannelType(deviceType, zone, pluginState.state);
    int channel = 0;
    if (midiChannelType == MidiChannelType::MPE_Low) channel = 1;
    else if (midiChannelType == MidiChannelType::MPE_High) channel = 16;
    else channel = (int)midiChannelType;
    
    strip1[zoneIdx].channel = channel;
    strip2[zoneIdx].channel = channel;

    strip1[zoneIdx].absMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip1Abs, ZoneWrapper::default_strip1Abs, pluginState.state);
    strip1[zoneIdx].relMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip1Rel, ZoneWrapper::default_strip1Rel, pluginState.state);
    strip2[zoneIdx].absMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip2Abs, ZoneWrapper::default_strip2Abs, pluginState.state);
    strip2[zoneIdx].relMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip2Rel, ZoneWrapper::default_strip2Rel, pluginState.state);
}

} // namespace ecm
