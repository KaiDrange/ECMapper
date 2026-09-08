#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "LayoutWrapper.h"
#include "PerformanceEvent.h"

namespace ecm {

class SettingsWrapper {
public:
    static inline const juce::Identifier id_globalSettings { "globalsettings" };
    static inline const juce::Identifier id_IP { "ipaddress" };
    static inline const juce::Identifier id_lowerMPEVoiceCount {"lowermpevoicecount"};
    static inline const juce::Identifier id_upperMPEVoiceCount {"uppermpevoicecount"};
    static inline const juce::Identifier id_lowerMPEPB {"lowermpepb"};
    static inline const juce::Identifier id_upperMPEPB {"uppermpepb"};
    static inline const juce::Identifier id_midi2Mode {"midi2Mode"};
    static inline const juce::Identifier id_pluginOutputMode {"pluginOutputMode"};
    static inline const juce::Identifier id_activeTab {"activetab"};
    
    static inline const juce::Identifier id_appRole { "appRole" };
    static inline const juce::Identifier id_clientListenIP { "clientListenIP" };
    static inline const juce::Identifier id_clientListenPort { "clientListenPort" };

    static inline const juce::Identifier id_calibration { "calibration" };
    static inline const juce::Identifier id_breathThreshold { "breathThreshold" };
    static inline const juce::Identifier id_breathSensitivity { "breathSensitivity" };
    static inline const juce::Identifier id_stripThreshold { "stripThreshold" };
    static inline const juce::Identifier id_stripSensitivity { "stripSensitivity" };
    static inline const juce::Identifier id_yawSensitivity { "yawSensitivity" };
    static inline const juce::Identifier id_rollSensitivity { "rollSensitivity" };
    static inline const juce::Identifier id_pressureSensitivity { "pressureSensitivity" };

    static void addListener(juce::ValueTree::Listener* listener, juce::ValueTree& rootState);

    static juce::String getIP(juce::ValueTree& rootState);
    static void setIP(juce::String ip, juce::ValueTree& rootState);
    static int getLowerMPEVoiceCount(juce::ValueTree& rootState);
    static void setLowerMPEVoiceCount(int count, juce::ValueTree& rootState);
    static int getUpperMPEVoiceCount(juce::ValueTree& rootState);
    static void setUpperMPEVoiceCount(int count, juce::ValueTree& rootState);
    static void setLowerMPEPB(int pbValue, juce::ValueTree& rootState);
    static int getLowerMPEPB(juce::ValueTree& rootState);
    static void setUpperMPEPB(int pbValue, juce::ValueTree& rootState);
    static int getUpperMPEPB(juce::ValueTree& rootState);
    static void setMidi2Mode(bool enabled, juce::ValueTree& rootState);
    static bool getMidi2Mode(juce::ValueTree& rootState);
    static void setPluginOutputMode(OutputTransportMode mode, juce::ValueTree& rootState);
    static OutputTransportMode getPluginOutputMode(juce::ValueTree& rootState);
    static void setCurrentTabIndex(int index, juce::ValueTree& rootState);
    static int getCurrentTabIndex(juce::ValueTree& rootState);
    
    static AppRole getAppRole(juce::ValueTree& rootState);
    static void setAppRole(AppRole role, juce::ValueTree& rootState);
    static juce::String getClientListenIP(juce::ValueTree& rootState);
    static void setClientListenIP(juce::String ip, juce::ValueTree& rootState);
    static int getClientListenPort(juce::ValueTree& rootState);
    static void setClientListenPort(int port, juce::ValueTree& rootState);
    
    static void setCalibrationValue(InstrumentType type, const juce::Identifier& param, float value, juce::ValueTree& rootState);
    static float getCalibrationValue(InstrumentType type, const juce::Identifier& param, float defaultValue, juce::ValueTree& rootState);
    static void resetCalibration(InstrumentType type, juce::ValueTree& rootState);
    
    static void saveDeviceSettings(const ConnectedDevice& device, juce::ValueTree& rootState);
    static void loadDeviceSettings(ConnectedDevice& device, juce::ValueTree& rootState);
    
    static juce::ValueTree getSettingsTree(juce::ValueTree& rootState);

private:
    static inline const juce::Identifier id_devices { "devices" };
    static inline const juce::Identifier id_mode { "mode" };
    static inline const juce::Identifier id_targets { "targets" };
    static inline const juce::Identifier id_target { "target" };
    static inline const juce::Identifier id_port { "port" };
    static inline const juce::Identifier id_receiveLEDs { "receiveLEDs" };
    static inline const juce::Identifier id_deviceNode { "device" };
    static inline const juce::Identifier id_devId { "devId" };
    static inline const juce::String default_IP { "127.0.0.1:12120" };
    static constexpr int default_lowerMPEVoiceCount = 15;
    static constexpr int default_upperMPEVoiceCount = 0;
    static constexpr int default_lowerMPEPB = 48;
    static constexpr int default_upperMPEPB = 48;
    static constexpr bool default_midi2Mode = false;
    static constexpr int default_pluginOutputMode = static_cast<int>(OutputTransportMode::LegacyMidi);
    static constexpr int default_activeTab = 0;

    static void cleanupLegacyDeviceNodes(juce::ValueTree& devicesNode);
};

} // namespace ecm
