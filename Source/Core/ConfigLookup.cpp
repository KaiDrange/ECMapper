#include "ConfigLookup.h"
#include <cmath>

namespace ecm {

namespace {

int getTransposeForZone(InstrumentType deviceType, Zone zone, juce::AudioProcessorValueTreeState& pluginState)
{
    auto paramId = ZoneWrapper::getTransposeParameterID(deviceType, zone);
    if (auto* raw = pluginState.getRawParameterValue(paramId))
        return (int) std::lround(raw->load());

    return ZoneWrapper::getTranspose(deviceType, zone, pluginState.state);
}

}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#endif
ConfigLookup::ConfigLookup(InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState, juce::CriticalSection& stateLock)
    : stateLock_(stateLock), deviceType(deviceType), pluginState(pluginState) {
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

ConfigLookup::ConfigLookup(const ConfigLookup& other)
    : stateLock_(other.stateLock_), deviceType(other.deviceType), pluginState(other.pluginState)
{
    controlLights = other.controlLights;
    for (int course = 0; course < 3; ++course) {
        for (int keyNo = 0; keyNo < 120; ++keyNo)
            keys[course][keyNo] = other.keys[course][keyNo];
    }

    for (int zone = 0; zone < 3; ++zone) {
        breath[zone] = other.breath[zone];
        strip1[zone] = other.strip1[zone];
        strip2[zone] = other.strip2[zone];
    }

    for (int i = 0; i < 6; ++i)
        expressionCurves[i] = other.expressionCurves[i];
}

void ConfigLookup::updateAll() {
    const juce::ScopedLock stateGuard(stateLock_);
    const juce::ScopedLock sl(lock_);
    jassert(deviceType != InstrumentType::None);
    this->controlLights = true;

    for (int course = 0; course < 3; ++course) {
        for (int keyNo = 0; keyNo < 120; ++keyNo) {
            updateKeyUnlocked({course, keyNo, deviceType});
        }
    }
    updateBreathUnlocked(Zone::Zone1);
    updateBreathUnlocked(Zone::Zone2);
    updateBreathUnlocked(Zone::Zone3);
    
    updateStripsUnlocked(Zone::Zone1);
    updateStripsUnlocked(Zone::Zone2);
    updateStripsUnlocked(Zone::Zone3);

    updateExpressionCurvesUnlocked();
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
    const juce::ScopedLock stateGuard(stateLock_);
    const juce::ScopedLock sl(lock_);
    jassert(keyId.deviceType != InstrumentType::None);
    updateKeyUnlocked(keyId);
}

void ConfigLookup::updateKeyUnlocked(LayoutWrapper::KeyId keyId) {
    LayoutWrapper::LayoutKey layoutKey = LayoutWrapper::getLayoutKey(keyId, pluginState.state);
    jassert(layoutKey.keyId.course >= 0 && layoutKey.keyId.course < 3);
    jassert(layoutKey.keyId.keyNo >= 0 && layoutKey.keyId.keyNo < 120);
    
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
        for (std::size_t i = 0; i < std::size(key.notes); ++i)
            key.notes[i] = -1;
            
        if (key.mapType == KeyMappingType::Chord) {
            juce::StringArray chordParts;
            Utils::splitString(layoutKey.mappingValue, ";", chordParts);
            if (chordParts.size() == 5) {
                for (std::size_t i = 0; i < std::size(key.notes); ++i) {
                    const int noteNumber = chordParts[static_cast<int>(i) + 1].getIntValue();
                    key.notes[i] = noteNumber < 0
                       ? -1
                       : std::clamp(noteNumber + getTransposeForZone(layoutKey.keyId.deviceType, layoutKey.zone, pluginState), 0, 127);
                }
            }
        }
        else {
            key.notes[0] = key.mapType == KeyMappingType::Note
                ? std::clamp(layoutKey.mappingValue.getIntValue() + getTransposeForZone(layoutKey.keyId.deviceType, layoutKey.zone, pluginState), 0, 127)
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

        bool midi2 = SettingsWrapper::getMidi2Mode(pluginState.state);
        float maxPb = 1.0f;

        if (key.output == MidiChannelType::MPE_Low)
            maxPb = (float)SettingsWrapper::getLowerMPEPB(pluginState.state);
        else if (key.output == MidiChannelType::MPE_High)
            maxPb = (float)SettingsWrapper::getUpperMPEPB(pluginState.state);
        else
            maxPb = (float)ZoneWrapper::getChannelMaxPitchbend(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state);

        key.pbRange = getSafePbRange((float)keyPB, maxPb);
        
        if (key.yaw.valueType == MidiValueType::Pitchbend || key.roll.valueType == MidiValueType::Pitchbend) {
             juce::Logger::writeToLog("ConfigLookup: PB Settings for Key [" + juce::String(layoutKey.keyId.course) + "," + juce::String(layoutKey.keyId.keyNo) + "] - midi2=" + juce::String((int)midi2) + 
                ", keyPB=" + juce::String(keyPB) + " semitones, maxPb=" + juce::String(maxPb) + 
                " semitones, pbScaling=" + juce::String(key.pbRange) + " (ratio)" +
                ", outputType=" + juce::String((int)key.output));
        }
        
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

        if (key.mapType != KeyMappingType::AppCtrl) {
            key.appCtrlType = 0;
            key.appCtrlValue = 0;
            key.cmdType = 0;
        }
        else {
            juce::StringArray appCtrlParts;
            Utils::splitString(layoutKey.mappingValue, ";", appCtrlParts);
            if (appCtrlParts.size() >= 2) {
                if (appCtrlParts[0] == "Preset") {
                    key.appCtrlType = 1;
                    key.appCtrlValue = appCtrlParts[1].getIntValue();
                    key.cmdType = 0;
                } else if (appCtrlParts[0] == "Transpose") {
                    key.appCtrlType = 2;
                    if (appCtrlParts.size() == 3) {
                        if (appCtrlParts[1] == "Latch") key.cmdType = 1;
                        else if (appCtrlParts[1] == "Momentary") key.cmdType = 2;
                        else if (appCtrlParts[1] == "Trigger") key.cmdType = 3;
                        else key.cmdType = 1;

                        key.appCtrlValue = appCtrlParts[2].getIntValue();
                    } else {
                        key.cmdType = 1;
                        key.appCtrlValue = appCtrlParts[1].getIntValue();
                    }
                }
            }
        }
    }
    if (layoutKey.keyId.course < 3 && layoutKey.keyId.keyNo < 120)
        keys[layoutKey.keyId.course][layoutKey.keyId.keyNo] = key;
}

void ConfigLookup::updateBreath(Zone zone) {
    const juce::ScopedLock stateGuard(stateLock_);
    const juce::ScopedLock sl(lock_);
    jassert(zone != Zone::NoZone);
    updateBreathUnlocked(zone);
}

void ConfigLookup::updateBreathUnlocked(Zone zone) {
    int zoneIdx = (int)zone - 1;
    jassert(zoneIdx >= 0 && zoneIdx < 3);
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
    
    float globalPB = 48.0f;
    if (midiChannelType == MidiChannelType::MPE_Low)
        globalPB = (float)SettingsWrapper::getLowerMPEPB(pluginState.state);
    else if (midiChannelType == MidiChannelType::MPE_High)
        globalPB = (float)SettingsWrapper::getUpperMPEPB(pluginState.state);
    else
        globalPB = (float)ZoneWrapper::getChannelMaxPitchbend(deviceType, zone, pluginState.state);

    breath[zoneIdx].pbRange = SettingsWrapper::getMidi2Mode(pluginState.state) ? (globalPB / 128.0f) : 1.0f;

    if (breath[zoneIdx].midiValue.valueType == MidiValueType::Pitchbend) {
        juce::Logger::writeToLog("ConfigLookup: PB Settings for Breath (Zone " + juce::String(zoneIdx) + ") - targetPB=" + juce::String(globalPB) + 
            " semitones, pbScaling=" + juce::String(breath[zoneIdx].pbRange));
    }
}

void ConfigLookup::updateStrips(Zone zone) {
    const juce::ScopedLock stateGuard(stateLock_);
    const juce::ScopedLock sl(lock_);
    jassert(zone != Zone::NoZone);
    updateStripsUnlocked(zone);
}

void ConfigLookup::updateStripsUnlocked(Zone zone) {
    int zoneIdx = (int)zone - 1;
    jassert(zoneIdx >= 0 && zoneIdx < 3);
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
    
    float globalPB = 48.0f;
    if (midiChannelType == MidiChannelType::MPE_Low)
        globalPB = (float)SettingsWrapper::getLowerMPEPB(pluginState.state);
    else if (midiChannelType == MidiChannelType::MPE_High)
        globalPB = (float)SettingsWrapper::getUpperMPEPB(pluginState.state);
    else
        globalPB = (float)ZoneWrapper::getChannelMaxPitchbend(deviceType, zone, pluginState.state);

    strip1[zoneIdx].pbRange = strip2[zoneIdx].pbRange = SettingsWrapper::getMidi2Mode(pluginState.state) ? (globalPB / 128.0f) : 1.0f;

    if (strip1[zoneIdx].absMidiValue.valueType == MidiValueType::Pitchbend || strip1[zoneIdx].relMidiValue.valueType == MidiValueType::Pitchbend) {
        juce::Logger::writeToLog("ConfigLookup: PB Settings for Strip 1 (Zone " + juce::String(zoneIdx) + ") - targetPB=" + juce::String(globalPB) + 
            " semitones, pbScaling=" + juce::String(strip1[zoneIdx].pbRange));
    }
    if (strip2[zoneIdx].absMidiValue.valueType == MidiValueType::Pitchbend || strip2[zoneIdx].relMidiValue.valueType == MidiValueType::Pitchbend) {
        juce::Logger::writeToLog("ConfigLookup: PB Settings for Strip 2 (Zone " + juce::String(zoneIdx) + ") - targetPB=" + juce::String(globalPB) + 
            " semitones, pbScaling=" + juce::String(strip2[zoneIdx].pbRange));
    }
}

void ConfigLookup::updateExpressionCurves() {
    const juce::ScopedLock stateGuard(stateLock_);
    const juce::ScopedLock sl(lock_);
    updateExpressionCurvesUnlocked();
}

void ConfigLookup::updateExpressionCurvesUnlocked() {
    for (int i = 0; i < 6; ++i) {
        auto target = static_cast<ExpressionCurveTarget>(i);
        expressionCurves[i] = ExpressionCurve(ExpressionCurveWrapper::getCurve(deviceType, target, pluginState.state).getData());
    }
}

} // namespace ecm
