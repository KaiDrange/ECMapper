#include <JuceHeader.h>

#include "Core/LayoutWrapper.h"
#include "Core/SettingsWrapper.h"

#include <iostream>
#include <vector>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

struct RecordingListener : juce::ValueTree::Listener
{
    std::vector<juce::Identifier> changedProperties;

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) override
    {
        changedProperties.push_back(property);
    }
};

bool containsProperty(const RecordingListener& listener, const juce::Identifier& property)
{
    return std::find(listener.changedProperties.begin(),
                     listener.changedProperties.end(),
                     property) != listener.changedProperties.end();
}

bool verifyPresetBackedSettingsNotifyListeners()
{
    juce::ValueTree rootState("ECMapperState");
    RecordingListener listener;
    ecm::SettingsWrapper::addListener(&listener, rootState);

    ecm::LayoutWrapper::LayoutKey layoutKey {
        { 0, 12, ecm::InstrumentType::Alpha },
        ecm::EigenharpKeyType::Perc,
        ecm::KeyColour::Red,
        ecm::Zone::Zone2,
        ecm::KeyMappingType::Note,
        "64"
    };
    ecm::LayoutWrapper::setLayoutKey(layoutKey, rootState);

    ecm::SettingsWrapper::setLowerMPEVoiceCount(9, rootState);
    ecm::SettingsWrapper::setUpperMPEVoiceCount(3, rootState);
    ecm::SettingsWrapper::setLowerMPEPB(24, rootState);
    ecm::SettingsWrapper::setUpperMPEPB(12, rootState);
    ecm::SettingsWrapper::setMidi2Mode(true, rootState);
    ecm::SettingsWrapper::setPluginOutputMode(ecm::OutputTransportMode::Vst3Direct, rootState);

    bool ok = true;
    ok &= expect(containsProperty(listener, ecm::SettingsWrapper::id_lowerMPEVoiceCount),
                 "changing lower MPE voices should notify settings listeners immediately");
    ok &= expect(containsProperty(listener, ecm::SettingsWrapper::id_upperMPEVoiceCount),
                 "changing upper MPE voices should notify settings listeners immediately");
    ok &= expect(containsProperty(listener, ecm::SettingsWrapper::id_lowerMPEPB),
                 "changing lower MPE pitch bend should notify settings listeners immediately");
    ok &= expect(containsProperty(listener, ecm::SettingsWrapper::id_upperMPEPB),
                 "changing upper MPE pitch bend should notify settings listeners immediately");
    ok &= expect(containsProperty(listener, ecm::SettingsWrapper::id_midi2Mode),
                 "changing MIDI 2.0 mode should still notify settings listeners immediately");
    ok &= expect(containsProperty(listener, ecm::SettingsWrapper::id_pluginOutputMode),
                 "changing plugin output mode should still notify settings listeners immediately");

    const auto restoredLayoutKey = ecm::LayoutWrapper::getLayoutKey(layoutKey.keyId, rootState);
    ok &= expect(restoredLayoutKey.zone == layoutKey.zone
                 && restoredLayoutKey.keyColour == layoutKey.keyColour
                 && restoredLayoutKey.keyMappingType == layoutKey.keyMappingType
                 && restoredLayoutKey.mappingValue == layoutKey.mappingValue,
                 "changing preset-backed MPE settings should not clear or overwrite existing layout mappings");

    ecm::SettingsWrapper::removeListener(&listener, rootState);
    return ok;
}

bool verifyGlobalSettingsStillNotifyListeners()
{
    juce::ValueTree rootState("ECMapperState");
    RecordingListener listener;
    ecm::SettingsWrapper::addListener(&listener, rootState);

    ecm::SettingsWrapper::setIP("127.0.0.1:12000", rootState);

    const bool ok = expect(containsProperty(listener, ecm::SettingsWrapper::id_IP),
                           "changing global settings should keep notifying listeners");

    ecm::SettingsWrapper::removeListener(&listener, rootState);
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= verifyPresetBackedSettingsNotifyListeners();
    ok &= verifyGlobalSettingsStillNotifyListeners();

    if (!ok)
        return 1;

    std::cout << "SettingsWrapperPresetListenerTest passed" << std::endl;
    return 0;
}