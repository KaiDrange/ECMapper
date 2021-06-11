#include "ConfigLookup.h"

ConfigLookup::ConfigLookup(DeviceType deviceType, juce::AudioProcessorValueTreeState &pluginState) : pluginState(pluginState) {
    this->deviceType = deviceType;
}

void ConfigLookup::updateAll() {
    juce::ValueTree layoutTree = LayoutWrapper::getLayoutTree(deviceType, pluginState.state);
    for (int i = 0; i < layoutTree.getNumChildren(); i++) {
        updateKey(layoutTree.getChild(i));
    }
    updateBreath(Zone::Zone1);
    updateBreath(Zone::Zone2);
    updateBreath(Zone::Zone3);
    
    updateStrips(Zone::Zone1);
    updateStrips(Zone::Zone2);
    updateStrips(Zone::Zone3);
}

void ConfigLookup::updateKey(juce::ValueTree keytree) {
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
    if (!ZoneWrapper::getEnabled(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state))
        setKeyToDefault = true;
    
    Key key;
    if (!setKeyToDefault) {
        key.keyType = layoutKey.keyType;
        key.mapType = layoutKey.keyMappingType;
        key.keyColour = layoutKey.keyColour;
        key.note = key.mapType == KeyMappingType::Note
            ? std::min(std::max(layoutKey.mappingValue.getIntValue() + ZoneWrapper::getTranspose(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state), 0), 127)
            : 0;
        key.pressure = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_pressure, ZoneWrapper::default_pressure, pluginState.state);
        key.roll = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_roll, ZoneWrapper::default_roll, pluginState.state);
        key.yaw = ZoneWrapper::getMidiValue(layoutKey.keyId.deviceType, layoutKey.zone, ZoneWrapper::id_yaw, ZoneWrapper::default_yaw, pluginState.state);
        key.output = ZoneWrapper::getMidiChannelType(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state);
        auto keyPB = ZoneWrapper::getKeyPitchbend(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state);
        if (key.output == MidiChannelType::MPE_Low)
            key.pbRange = std::min(((float)keyPB)/((float)SettingsWrapper::getLowerMPEPB(pluginState.state)), 1.0f);
        else if (key.output == MidiChannelType::MPE_High)
            key.pbRange = std::min(((float)keyPB)/((float)SettingsWrapper::getUpperMPEPB(pluginState.state)), 1.0f);
        else
            key.pbRange = std::min(((float)keyPB)/((float)ZoneWrapper::getChannelMaxPitchbend(layoutKey.keyId.deviceType, layoutKey.zone, pluginState.state)), 1.0f);
        
        if (key.mapType != KeyMappingType::MidiMsg) {
            key.cmdType = 0;
            key.msgType = 0;
            key.cmdCC = 0;
            key.cmdOn = 0;
            key.cmdOff = 0;
        }
        else {
            juce::StringArray cmdParts;
            Utility::splitString(layoutKey.mappingValue, ";", cmdParts);
            if (cmdParts.size() == 5) {
                if (cmdParts[0] == "Latch")
                    key.cmdType = 1;
                else if (cmdParts[0] == "Momentary")
                    key.cmdType = 2;
                else if (cmdParts[0] == "Trigger")
                    key.cmdType = 3;
                else
                    key.cmdType = 0;

                if (cmdParts[1] == "CC")
                    key.msgType = 1;
                else if (cmdParts[1] == "PC")
                    key.msgType = 2;
                else if (cmdParts[1] == "Realtime")
                    key.msgType = 3;
                else if (cmdParts[1] == "AllNotesOff")
                    key.msgType = 4;
                else
                    key.msgType = 0;
                
                key.cmdCC = cmdParts[2].getIntValue();
                key.cmdOff = cmdParts[3].getIntValue();
                key.cmdOn = cmdParts[4].getIntValue();
            }
        }
    }
    keys[layoutKey.keyId.course][layoutKey.keyId.keyNo] = key;
}

void ConfigLookup::updateBreath(Zone zone) {
    if (!ZoneWrapper::getEnabled(deviceType, zone, pluginState.state)) {
        breath[((int)zone)-1].channel = 0;
        breath[((int)zone)-1].midiValue.valueType = MidiValueType::Off;
        return;
    }
    
    auto midiChannelType = ZoneWrapper::getMidiChannelType(deviceType, zone, pluginState.state);
    if (midiChannelType == MidiChannelType::MPE_Low)
        breath[((int)zone)-1].channel = 1;
    else if (midiChannelType == MidiChannelType::MPE_High)
        breath[((int)zone)-1].channel = 16;
    else
        breath[((int)zone)-1].channel = (int)midiChannelType;
    
    breath[((int)zone)-1].midiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_breath, ZoneWrapper::default_breath, pluginState.state);
}

void ConfigLookup::updateStrips(Zone zone) {
    if (!ZoneWrapper::getEnabled(deviceType, zone, pluginState.state)) {
        strip1[((int)zone)-1].channel = 0;
        strip2[((int)zone)-1].channel = 0;
        strip1[((int)zone)-1].absMidiValue.valueType = MidiValueType::Off;
        strip1[((int)zone)-1].relMidiValue.valueType = MidiValueType::Off;
        strip2[((int)zone)-1].absMidiValue.valueType = MidiValueType::Off;
        strip2[((int)zone)-1].relMidiValue.valueType = MidiValueType::Off;
        return;
    }
    
    auto midiChannelType = ZoneWrapper::getMidiChannelType(deviceType, zone, pluginState.state);
    if (midiChannelType == MidiChannelType::MPE_Low) {
        strip1[((int)zone)-1].channel = 1;
        strip2[((int)zone)-1].channel = 1;
    }
    else if (midiChannelType == MidiChannelType::MPE_High) {
        strip1[((int)zone)-1].channel = 16;
        strip2[((int)zone)-1].channel = 16;
    }
    else {
        strip1[((int)zone)-1].channel = (int)midiChannelType;
        strip2[((int)zone)-1].channel = (int)midiChannelType;
    }

    strip1[((int)zone)-1].absMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip1Abs, ZoneWrapper::default_strip1Abs, pluginState.state);
    strip1[((int)zone)-1].relMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip1Rel, ZoneWrapper::default_strip1Rel, pluginState.state);
    strip2[((int)zone)-1].absMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip2Abs, ZoneWrapper::default_strip2Abs, pluginState.state);
    strip2[((int)zone)-1].relMidiValue = ZoneWrapper::getMidiValue(deviceType, zone, ZoneWrapper::id_strip2Rel, ZoneWrapper::default_strip2Rel, pluginState.state);
}
