#include "SettingsWrapper.h"

void SettingsWrapper::addListener(juce::ValueTree::Listener *listener) {
    auto vTree = getSettingsTree();
    vTree.addListener(listener);
}

juce::String SettingsWrapper::getIP() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_IP, default_IP);
}

void SettingsWrapper::setIP(juce::String ip) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_IP, ip, nullptr);
}

juce::ValueTree SettingsWrapper::getSettingsTree() {
    return rootState->getOrCreateChildWithName(id_globalSettings, nullptr);
}

void SettingsWrapper::setLowerMPEVoiceCount(int channel) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_lowerMPEVoiceCount, channel, nullptr);
}

int SettingsWrapper::getLowerMPEVoiceCount() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_lowerMPEVoiceCount, default_lowerMPEVoiceCount);
}

void SettingsWrapper::setUpperMPEVoiceCount(int channel) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_upperMPEVoiceCount, channel, nullptr);
}

int SettingsWrapper::getUpperMPEVoiceCount() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_upperMPEVoiceCount, default_upperMPEVoiceCount);
}

void SettingsWrapper::setLowerMPEPB(int pbValue) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_lowerMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getLowerMPEPB() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_lowerMPEPB, default_lowerMPEPB);
}

void SettingsWrapper::setUpperMPEPB(int pbValue) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_upperMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getUpperMPEPB() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_upperMPEPB, default_upperMPEPB);
}
