#include "SettingsWrapper.h"

namespace ecm {

bool SettingsWrapper::isLegacyPresetProperty(const juce::Identifier& property) {
    return property == id_lowerMPEVoiceCount
        || property == id_upperMPEVoiceCount
        || property == id_lowerMPEPB
        || property == id_upperMPEPB
        || property == id_midi2Mode
        || property == id_pluginOutputMode;
}

void SettingsWrapper::mergeLegacyTreeIntoPresetTree(juce::ValueTree& targetTree, const juce::ValueTree& legacyTree) {
    if (!targetTree.isValid() || !legacyTree.isValid())
        return;

    for (int propertyIndex = 0; propertyIndex < legacyTree.getNumProperties(); ++propertyIndex) {
        const auto property = legacyTree.getPropertyName(propertyIndex);
        if (!targetTree.hasProperty(property))
            targetTree.setProperty(property, legacyTree.getProperty(property), nullptr);
    }

    for (int childIndex = 0; childIndex < legacyTree.getNumChildren(); ++childIndex) {
        auto legacyChild = legacyTree.getChild(childIndex);
        auto existingChild = targetTree.getChildWithName(legacyChild.getType());

        if (existingChild.isValid()) {
            mergeLegacyTreeIntoPresetTree(existingChild, legacyChild);
        } else {
            targetTree.addChild(legacyChild.createCopy(), -1, nullptr);
        }
    }
}

void SettingsWrapper::addListener(juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    rootState.addListener(listener);

    auto settingsTree = getSettingsTree(rootState);
    if (settingsTree != rootState)
        settingsTree.addListener(listener);

    auto presetTree = getPresetTree(rootState);
    if (presetTree != rootState && presetTree != settingsTree)
        presetTree.addListener(listener);
}

void SettingsWrapper::removeListener(juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    rootState.removeListener(listener);

    auto settingsTree = getSettingsTree(rootState);
    if (settingsTree != rootState)
        settingsTree.removeListener(listener);

    auto presetTree = getPresetTree(rootState);
    if (presetTree != rootState && presetTree != settingsTree)
        presetTree.removeListener(listener);
}

void SettingsWrapper::cleanupLegacyDeviceNodes(juce::ValueTree& devicesNode) {
    if (!devicesNode.isValid()) return;
    for (int i = devicesNode.getNumChildren(); --i >= 0;) {
        auto child = devicesNode.getChild(i);
        if (child.getType() != id_deviceNode) {
            devicesNode.removeChild(i, nullptr);
        }
    }
}

juce::ValueTree SettingsWrapper::getSettingsTree(juce::ValueTree& rootState) {
    normalizeStateTree(rootState);
    auto vTree = rootState.getOrCreateChildWithName(id_globalSettings, nullptr);
    auto devices = vTree.getOrCreateChildWithName(id_devices, nullptr);
    cleanupLegacyDeviceNodes(devices);
    return vTree;
}

juce::ValueTree SettingsWrapper::getPresetTree(juce::ValueTree& rootState) {
    normalizeStateTree(rootState);
    auto presetTree = rootState.getOrCreateChildWithName(id_preset, nullptr);
    if (!presetTree.hasProperty(id_ecMapperVersion))
        presetTree.setProperty(id_ecMapperVersion, ProjectInfo::versionString, nullptr);
    return presetTree;
}

void SettingsWrapper::migrateLegacyPresetProperties(juce::ValueTree& rootState, juce::ValueTree& presetTree) {
    auto globalSettings = rootState.getChildWithName(id_globalSettings);
    if (!globalSettings.isValid())
        return;

    for (int i = globalSettings.getNumProperties(); --i >= 0;) {
        const auto property = globalSettings.getPropertyName(i);
        if (!isLegacyPresetProperty(property) || presetTree.hasProperty(property))
            continue;

        presetTree.setProperty(property, globalSettings.getProperty(property), nullptr);
        globalSettings.removeProperty(property, nullptr);
    }
}

void SettingsWrapper::migrateLegacyDeviceNodes(juce::ValueTree& rootState, juce::ValueTree& presetTree) {
    for (int i = rootState.getNumChildren(); --i >= 0;) {
        auto child = rootState.getChild(i);
        const auto type = child.getType().toString();
        if (!type.startsWith(LayoutWrapper::id_device.toString()))
            continue;

        auto existing = presetTree.getChildWithName(child.getType());
        if (existing.isValid()) {
            mergeLegacyTreeIntoPresetTree(existing, child);
        } else {
            presetTree.addChild(child.createCopy(), -1, nullptr);
        }

        rootState.removeChild(i, nullptr);
    }
}

void SettingsWrapper::normalizeStateTree(juce::ValueTree& rootState) {
    if (!rootState.isValid())
        return;

    auto presetTree = rootState.getOrCreateChildWithName(id_preset, nullptr);
    migrateLegacyPresetProperties(rootState, presetTree);
    migrateLegacyDeviceNodes(rootState, presetTree);

    if (!rootState.hasProperty(id_ecMapperVersion))
        rootState.setProperty(id_ecMapperVersion, ProjectInfo::versionString, nullptr);
    if (!presetTree.hasProperty(id_ecMapperVersion))
        presetTree.setProperty(id_ecMapperVersion, ProjectInfo::versionString, nullptr);
}

juce::ValueTree SettingsWrapper::createPersistentStateTree(juce::ValueTree& rootState) {
    auto stateCopy = rootState.createCopy();
    normalizeStateTree(stateCopy);
    stateCopy.setProperty(id_ecMapperVersion, ProjectInfo::versionString, nullptr);
    auto presetTree = stateCopy.getOrCreateChildWithName(id_preset, nullptr);
    presetTree.setProperty(id_ecMapperVersion, ProjectInfo::versionString, nullptr);
    return stateCopy;
}

juce::String SettingsWrapper::getIP(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_IP, default_IP);
}

void SettingsWrapper::setIP(juce::String ip, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_IP, ip, nullptr);
}

void SettingsWrapper::setLowerMPEVoiceCount(int count, juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    vTree.setProperty(id_lowerMPEVoiceCount, count, nullptr);
}

int SettingsWrapper::getLowerMPEVoiceCount(juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    return vTree.getProperty(id_lowerMPEVoiceCount, default_lowerMPEVoiceCount);
}

void SettingsWrapper::setUpperMPEVoiceCount(int count, juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    vTree.setProperty(id_upperMPEVoiceCount, count, nullptr);
}

int SettingsWrapper::getUpperMPEVoiceCount(juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    return vTree.getProperty(id_upperMPEVoiceCount, default_upperMPEVoiceCount);
}

void SettingsWrapper::setLowerMPEPB(int pbValue, juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    vTree.setProperty(id_lowerMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getLowerMPEPB(juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    return vTree.getProperty(id_lowerMPEPB, default_lowerMPEPB);
}

void SettingsWrapper::setUpperMPEPB(int pbValue, juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    vTree.setProperty(id_upperMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getUpperMPEPB(juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    return vTree.getProperty(id_upperMPEPB, default_upperMPEPB);
}

void SettingsWrapper::setMidi2Mode(bool enabled, juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    vTree.setProperty(id_midi2Mode, enabled, nullptr);
    juce::Logger::writeToLog("SettingsWrapper: setMidi2Mode to " + juce::String((int)enabled));
}

bool SettingsWrapper::getMidi2Mode(juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    return vTree.getProperty(id_midi2Mode, default_midi2Mode);
}

void SettingsWrapper::setPluginOutputMode(OutputTransportMode mode, juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    vTree.setProperty(id_pluginOutputMode, static_cast<int>(mode), nullptr);
}

OutputTransportMode SettingsWrapper::getPluginOutputMode(juce::ValueTree& rootState) {
    auto vTree = getPresetTree(rootState);
    const int stored = static_cast<int>(vTree.getProperty(id_pluginOutputMode, default_pluginOutputMode));
    if (stored == static_cast<int>(OutputTransportMode::Vst3Direct))
        return OutputTransportMode::Vst3Direct;
    return OutputTransportMode::LegacyMidi;
}

void SettingsWrapper::setCurrentTabIndex(int index, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_activeTab, index, nullptr);
}

int SettingsWrapper::getCurrentTabIndex(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_activeTab, default_activeTab);
}

void SettingsWrapper::setCurrentCalibrationTabIndex(int index, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_activeCalibrationTab, index, nullptr);
}

int SettingsWrapper::getCurrentCalibrationTabIndex(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_activeCalibrationTab, default_activeCalibrationTab);
}

AppRole SettingsWrapper::getAppRole(juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    return (AppRole)(int)settings.getProperty(id_appRole, (int)AppRole::Host);
}

void SettingsWrapper::setAppRole(AppRole role, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    settings.setProperty(id_appRole, (int)role, nullptr);
}

juce::String SettingsWrapper::getClientListenIP(juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    return settings.getProperty(id_clientListenIP, "127.0.0.1").toString();
}

void SettingsWrapper::setClientListenIP(juce::String ip, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    settings.setProperty(id_clientListenIP, ip, nullptr);
}

int SettingsWrapper::getClientListenPort(juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    return settings.getProperty(id_clientListenPort, 12130);
}

void SettingsWrapper::setClientListenPort(int port, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    settings.setProperty(id_clientListenPort, port, nullptr);
}

static juce::String getDeviceNodeName(InstrumentType type) {
    switch (type) {
        case InstrumentType::Pico: return "Pico";
        case InstrumentType::Tau: return "Tau";
        case InstrumentType::Alpha: return "Alpha";
        default: return "Unknown";
    }
}

void SettingsWrapper::setCalibrationValue(InstrumentType type, const juce::Identifier& param, float value, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto calibration = settings.getOrCreateChildWithName(id_calibration, nullptr);
    auto devNode = calibration.getOrCreateChildWithName(getDeviceNodeName(type), nullptr);
    devNode.setProperty(param, value, nullptr);
    rootState.setProperty(id_calibrationRevision, static_cast<int>(rootState.getProperty(id_calibrationRevision, 0)) + 1, nullptr);
}

float SettingsWrapper::getCalibrationValue(InstrumentType type, const juce::Identifier& param, float defaultValue, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto calibration = settings.getChildWithName(id_calibration);
    if (!calibration.isValid()) return defaultValue;
    auto devNode = calibration.getChildWithName(getDeviceNodeName(type));
    if (!devNode.isValid()) return defaultValue;
    return devNode.getProperty(param, defaultValue);
}

void SettingsWrapper::setCalibrationBool(InstrumentType type, const juce::Identifier& param, bool value, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto calibration = settings.getOrCreateChildWithName(id_calibration, nullptr);
    auto devNode = calibration.getOrCreateChildWithName(getDeviceNodeName(type), nullptr);
    devNode.setProperty(param, value, nullptr);
    rootState.setProperty(id_calibrationRevision, static_cast<int>(rootState.getProperty(id_calibrationRevision, 0)) + 1, nullptr);
}

bool SettingsWrapper::getCalibrationBool(InstrumentType type, const juce::Identifier& param, bool defaultValue, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto calibration = settings.getChildWithName(id_calibration);
    if (!calibration.isValid()) return defaultValue;
    auto devNode = calibration.getChildWithName(getDeviceNodeName(type));
    if (!devNode.isValid()) return defaultValue;
    return devNode.getProperty(param, defaultValue);
}

void SettingsWrapper::resetCalibration(InstrumentType type, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto calibration = settings.getChildWithName(id_calibration);
    if (calibration.isValid()) {
        auto devNode = calibration.getChildWithName(getDeviceNodeName(type));
        if (devNode.isValid()) {
            calibration.removeChild(devNode, nullptr);
            rootState.setProperty(id_calibrationRevision, static_cast<int>(rootState.getProperty(id_calibrationRevision, 0)) + 1, nullptr);
        }
    }
}

void SettingsWrapper::saveDeviceSettings(const ConnectedDevice& device, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto devices = settings.getOrCreateChildWithName(id_devices, nullptr);
    
    juce::String devId = device.isRemote ? juce::String(device.remoteOriginalDevId) : juce::String(device.dev);
    if (devId.isEmpty()) return;
    
    auto devNode = devices.getChildWithProperty(id_devId, devId);
    if (!devNode.isValid()) {
        devNode = juce::ValueTree(id_deviceNode);
        devNode.setProperty(id_devId, devId, nullptr);
        devices.appendChild(devNode, nullptr);
    }
    
    devNode.setProperty(id_mode, (int)device.mode, nullptr);
    
    devNode.removeChild(devNode.getChildWithName(id_targets), nullptr);
    auto targetsNode = devNode.getOrCreateChildWithName(id_targets, nullptr);
    
    for (const auto& t : device.oscTargets) {
        juce::ValueTree tNode(id_target);
        tNode.setProperty(id_IP, t.ip, nullptr);
        tNode.setProperty(id_port, t.port, nullptr);
        tNode.setProperty(id_receiveLEDs, t.receiveLEDs, nullptr);
        targetsNode.appendChild(tNode, nullptr);
    }
}

void SettingsWrapper::loadDeviceSettings(ConnectedDevice& device, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    auto devices = settings.getChildWithName(id_devices);
    if (!devices.isValid()) return;
    
    juce::String devId = device.isRemote ? juce::String(device.remoteOriginalDevId) : juce::String(device.dev);
    if (devId.isEmpty()) return;
    
    auto devNode = devices.getChildWithProperty(id_devId, devId);
    if (!devNode.isValid()) return;
    
    device.mode = (DeviceMode)(int)devNode.getProperty(id_mode, (int)DeviceMode::Local);
    
    auto targetsNode = devNode.getChildWithName(id_targets);
    if (targetsNode.isValid()) {
        device.oscTargets.clear();
        for (int i = 0; i < targetsNode.getNumChildren(); ++i) {
            auto tNode = targetsNode.getChild(i);
            OSCTarget t;
            t.ip = tNode.getProperty(id_IP, "127.0.0.1");
            t.port = tNode.getProperty(id_port, 12120);
            t.receiveLEDs = tNode.getProperty(id_receiveLEDs, false);
            device.oscTargets.push_back(t);
        }
    }
}

} // namespace ecm
