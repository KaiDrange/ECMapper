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

void SettingsWrapper::setLowMPEToChannel(int channel) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_lowMPEToChannel, channel, nullptr);
}

int SettingsWrapper::getLowMPEToChannel() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_lowMPEToChannel, default_lowMPEToChannel);
}

void SettingsWrapper::setLowMPEPB(int pbValue) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_lowMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getLowMPEPB() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_lowMPEPB, default_lowMPEPB);
}

void SettingsWrapper::setHighMPEPB(int pbValue) {
    auto vTree = getSettingsTree();
    vTree.setProperty(id_highMPEPB, pbValue, nullptr);
}

int SettingsWrapper::getHighMPEPB() {
    auto vTree = getSettingsTree();
    return vTree.getProperty(id_highMPEPB, default_highMPEPB);
}
