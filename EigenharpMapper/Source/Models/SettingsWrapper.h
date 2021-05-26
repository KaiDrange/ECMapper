#pragma once
#include <JuceHeader.h>

extern juce::ValueTree *rootState;

class SettingsWrapper {
public:
    static inline const juce::Identifier id_globalSettings { "globalsettings" };
    static inline const juce::Identifier id_IP { "ipaddress" };
    static inline const juce::Identifier id_lowMPEToChannel {"lowmpetochannel"};
    static inline const juce::Identifier id_lowMPEPB {"lowmpepb"};
    static inline const juce::Identifier id_highMPEPB {"highmpepb"};

    static void addListener(juce::ValueTree::Listener *listener);

    static juce::String getIP();
    static void setIP(juce::String ip);
    static int getLowMPEToChannel();
    static void setLowMPEToChannel(int channel);
    static void setLowMPEPB(int pbValue);
    static int getLowMPEPB();
    static void setHighMPEPB(int pbValue);
    static int getHighMPEPB();

    
private:
    static inline const juce::String default_IP { "127.0.0.1:7000" };
    static inline const int default_lowMPEToChannel = 14;
    static inline const int default_lowMPEPB = 48;
    static inline const int default_highMPEPB = 48;

    static juce::ValueTree getSettingsTree();
    
};
