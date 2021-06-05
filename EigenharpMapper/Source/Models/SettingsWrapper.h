#pragma once
#include <JuceHeader.h>

extern juce::ValueTree *rootState;

class SettingsWrapper {
public:
    static inline const juce::Identifier id_globalSettings { "globalsettings" };
    static inline const juce::Identifier id_IP { "ipaddress" };
    static inline const juce::Identifier id_lowerMPEVoiceCount {"lowermpevoicecount"};
    static inline const juce::Identifier id_upperMPEVoiceCount {"uppermpevoicecount"};
    static inline const juce::Identifier id_lowerMPEPB {"lowermpepb"};
    static inline const juce::Identifier id_upperMPEPB {"uppermpepb"};

    static void addListener(juce::ValueTree::Listener *listener);

    static juce::String getIP();
    static void setIP(juce::String ip);
    static int getLowerMPEVoiceCount();
    static void setLowerMPEVoiceCount(int channel);
    static int getUpperMPEVoiceCount();
    static void setUpperMPEVoiceCount(int channel);
    static void setLowerMPEPB(int pbValue);
    static int getLowerMPEPB();
    static void setUpperMPEPB(int pbValue);
    static int getUpperMPEPB();

    
private:
    static inline const juce::String default_IP { "127.0.0.1:12120" };
    static inline const int default_lowerMPEVoiceCount = 15;
    static inline const int default_upperMPEVoiceCount = 0;
    static inline const int default_lowerMPEPB = 48;
    static inline const int default_upperMPEPB = 48;

    static juce::ValueTree getSettingsTree();
    
};
