#include "SettingsWrapper.h"

namespace ecm {

void SettingsWrapper::addListener(juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.addListener(listener);
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
    auto vTree = rootState.getOrCreateChildWithName(id_globalSettings, nullptr);
    auto devices = vTree.getOrCreateChildWithName(id_devices, nullptr);
    cleanupLegacyDeviceNodes(devices);
    return vTree;
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
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_lowerMPEVoiceCount, count, nullptr);
}

int SettingsWrapper::getLowerMPEVoiceCount(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_lowerMPEVoiceCount, default_lowerMPEVoiceCount);
}

void SettingsWrapper::setUpperMPEVoiceCount(int count, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_upperMPEVoiceCount, count, nullptr);
}

int SettingsWrapper::getUpperMPEVoiceCount(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_upperMPEVoiceCount, default_upperMPEVoiceCount);
}

void SettingsWrapper::setLowerMPEPB(int pbValue, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_lowerMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getLowerMPEPB(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_lowerMPEPB, default_lowerMPEPB);
}

void SettingsWrapper::setUpperMPEPB(int pbValue, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_upperMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getUpperMPEPB(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_upperMPEPB, default_upperMPEPB);
}

void SettingsWrapper::setMidi2Mode(bool enabled, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_midi2Mode, enabled, nullptr);
}

bool SettingsWrapper::getMidi2Mode(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_midi2Mode, default_midi2Mode);
}

void SettingsWrapper::setCurrentTabIndex(int index, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_activeTab, index, nullptr);
}

int SettingsWrapper::getCurrentTabIndex(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_activeTab, default_activeTab);
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
