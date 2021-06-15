#include "SettingsWrapper.h"

void SettingsWrapper::addListener(juce::ValueTree::Listener *listener, juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.addListener(listener);
}

juce::String SettingsWrapper::getIP(juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_IP, default_IP);
}

void SettingsWrapper::setIP(juce::String ip, juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_IP, ip, nullptr);
}

juce::ValueTree SettingsWrapper::getSettingsTree(juce::ValueTree &rootState) {
    return rootState.getOrCreateChildWithName(id_globalSettings, nullptr);
}

void SettingsWrapper::setLowerMPEVoiceCount(int channel, juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_lowerMPEVoiceCount, channel, nullptr);
}

int SettingsWrapper::getLowerMPEVoiceCount(juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_lowerMPEVoiceCount, default_lowerMPEVoiceCount);
}

void SettingsWrapper::setUpperMPEVoiceCount(int channel, juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_upperMPEVoiceCount, channel, nullptr);
}

int SettingsWrapper::getUpperMPEVoiceCount(juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_upperMPEVoiceCount, default_upperMPEVoiceCount);
}

void SettingsWrapper::setLowerMPEPB(int pbValue,juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_lowerMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getLowerMPEPB(juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_lowerMPEPB, default_lowerMPEPB);
}

void SettingsWrapper::setUpperMPEPB(int pbValue, juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_upperMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getUpperMPEPB(juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_upperMPEPB, default_upperMPEPB);
}

void SettingsWrapper::setCurrentTabIndex(int index, juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    vTree.setProperty(id_activeTab, index, nullptr);
}

int SettingsWrapper::getCurrentTabIndex(juce::ValueTree &rootState) {
    auto vTree = getSettingsTree(rootState);
    return vTree.getProperty(id_activeTab, default_activeTab);
}
