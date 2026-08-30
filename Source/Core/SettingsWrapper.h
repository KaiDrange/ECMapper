#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "LayoutWrapper.h"

namespace ecm {

class SettingsWrapper {
public:
    static inline const juce::Identifier id_globalSettings { "globalsettings" };
    static inline const juce::Identifier id_IP { "ipaddress" };
    static inline const juce::Identifier id_lowerMPEVoiceCount {"lowermpevoicecount"};
    static inline const juce::Identifier id_upperMPEVoiceCount {"uppermpevoicecount"};
    static inline const juce::Identifier id_lowerMPEPB {"lowermpepb"};
    static inline const juce::Identifier id_upperMPEPB {"uppermpepb"};
    static inline const juce::Identifier id_activeTab {"activetab"};
    static inline const juce::Identifier id_controlLights { "controlLights" };
    
    static inline const juce::Identifier id_appRole { "appRole" };
    static inline const juce::Identifier id_clientListenIP { "clientListenIP" };
    static inline const juce::Identifier id_clientListenPort { "clientListenPort" };

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
    static void setCurrentTabIndex(int index, juce::ValueTree& rootState);
    static int getCurrentTabIndex(juce::ValueTree& rootState);
    
    static bool getControlLights(InstrumentType deviceType, juce::ValueTree& rootState);
    static void setControlLights(bool value, InstrumentType deviceType, juce::ValueTree& rootState);
    
    static AppRole getAppRole(juce::ValueTree& rootState);
    static void setAppRole(AppRole role, juce::ValueTree& rootState);
    static juce::String getClientListenIP(juce::ValueTree& rootState);
    static void setClientListenIP(juce::String ip, juce::ValueTree& rootState);
    static int getClientListenPort(juce::ValueTree& rootState);
    static void setClientListenPort(int port, juce::ValueTree& rootState);
    
    static void saveDeviceSettings(const ConnectedDevice& device, juce::ValueTree& rootState);
    static void loadDeviceSettings(ConnectedDevice& device, juce::ValueTree& rootState);
    
private:
    static inline const juce::Identifier id_devices { "devices" };
    static inline const juce::Identifier id_mode { "mode" };
    static inline const juce::Identifier id_targets { "targets" };
    static inline const juce::Identifier id_target { "target" };
    static inline const juce::Identifier id_port { "port" };
    static inline const juce::Identifier id_deviceNode { "device" };
    static inline const juce::Identifier id_devId { "devId" };
    static inline const juce::String default_IP { "127.0.0.1:12120" };
    static constexpr int default_lowerMPEVoiceCount = 15;
    static constexpr int default_upperMPEVoiceCount = 0;
    static constexpr int default_lowerMPEPB = 48;
    static constexpr int default_upperMPEPB = 48;
    static constexpr int default_activeTab = 0;

    static juce::ValueTree getSettingsTree(juce::ValueTree& rootState);
};

} // namespace ecm
