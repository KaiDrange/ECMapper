#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "LayoutWrapper.h"

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

    static void addListener(juce::ValueTree::Listener *listener, juce::ValueTree &rootState);

    static juce::String getIP(juce::ValueTree &rootState);
    static void setIP(juce::String ip, juce::ValueTree &rootState);
    static int getLowerMPEVoiceCount(juce::ValueTree &rootState);
    static void setLowerMPEVoiceCount(int channel, juce::ValueTree &rootState);
    static int getUpperMPEVoiceCount(juce::ValueTree &rootState);
    static void setUpperMPEVoiceCount(int channel, juce::ValueTree &rootState);
    static void setLowerMPEPB(int pbValue, juce::ValueTree &rootState);
    static int getLowerMPEPB(juce::ValueTree &rootState);
    static void setUpperMPEPB(int pbValue, juce::ValueTree &rootState);
    static int getUpperMPEPB(juce::ValueTree &rootState);
    static void setCurrentTabIndex(int index, juce::ValueTree &rootState);
    static int getCurrentTabIndex(juce::ValueTree &rootState);
    
    static bool getControlLights(DeviceType deviceType, juce::ValueTree &rootState);
    static void setControlLights(bool value, DeviceType deviceType, juce::ValueTree &rootState);
    
private:
    static inline const juce::String default_IP { "127.0.0.1:12120" };
    static inline const int default_lowerMPEVoiceCount = 15;
    static inline const int default_upperMPEVoiceCount = 0;
    static inline const int default_lowerMPEPB = 48;
    static inline const int default_upperMPEPB = 48;
    static inline const int default_activeTab = 0;

    static juce::ValueTree getSettingsTree(juce::ValueTree &rootState);
    
};
