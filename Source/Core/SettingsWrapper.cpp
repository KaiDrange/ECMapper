#include "SettingsWrapper.h"

namespace ecm {

void SettingsWrapper::addListener(juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.addListener(listener);
}

juce::ValueTree SettingsWrapper::getSettingsTree(juce::ValueTree& rootState) {
    return rootState.getOrCreateChildWithName(id_globalSettings, nullptr);
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

void SettingsWrapper::setCurrentTabIndex(int index, juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_activeTab, index, nullptr);
}

int SettingsWrapper::getCurrentTabIndex(juce::ValueTree& rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_activeTab, default_activeTab);
}

bool SettingsWrapper::getControlLights(InstrumentType deviceType, juce::ValueTree& rootState) {
    auto deviceChild = rootState.getOrCreateChildWithName(LayoutWrapper::id_device + juce::String((int)deviceType), nullptr);
    return deviceChild.getProperty(id_controlLights, true);
}

void SettingsWrapper::setControlLights(bool value, InstrumentType deviceType, juce::ValueTree& rootState) {
    auto deviceChild = rootState.getOrCreateChildWithName(LayoutWrapper::id_device + juce::String((int)deviceType), nullptr);
    deviceChild.setProperty(id_controlLights, value, nullptr);
}

AppRole SettingsWrapper::getAppRole(juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    return (AppRole)(int)settings.getProperty(id_appRole, (int)AppRole::Master);
}

void SettingsWrapper::setAppRole(AppRole role, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    settings.setProperty(id_appRole, (int)role, nullptr);
}

juce::String SettingsWrapper::getSlaveListenIP(juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    return settings.getProperty(id_slaveListenIP, "127.0.0.1").toString();
}

void SettingsWrapper::setSlaveListenIP(juce::String ip, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    settings.setProperty(id_slaveListenIP, ip, nullptr);
}

int SettingsWrapper::getSlaveListenPort(juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    return settings.getProperty(id_slaveListenPort, 12130);
}

void SettingsWrapper::setSlaveListenPort(int port, juce::ValueTree& rootState) {
    auto settings = getSettingsTree(rootState);
    settings.setProperty(id_slaveListenPort, port, nullptr);
}

} // namespace ecm
