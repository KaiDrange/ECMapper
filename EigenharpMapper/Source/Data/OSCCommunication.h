#pragma once

#include <JuceHeader.h>
#include <iostream>


class OSCCommunication : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, juce::Timer {
public:
    OSCCommunication();
    ~OSCCommunication();
    bool connectSender(juce::String ip, int port);
    void disconnectSender();
    bool connectReceiver(int port);
    void disconnectReceiver();
    
    void sendLED(int course, int key, int led);
private:
    juce::OSCSender sender;
    juce::String senderIP;
    int senderPort = -1;
    
    juce::OSCReceiver receiver;
    juce::String receiverIP;
    int receiverPort = -1;
    
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void timerCallback() override;
};
