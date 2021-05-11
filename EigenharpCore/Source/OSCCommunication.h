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
    
    void sendKey(unsigned course, unsigned key, bool a, unsigned p, int r, int y);
    void sendDeviceName(const juce::String &name);
    void sendBreath(unsigned val);
    void sendStrip(unsigned strip, unsigned val);
    void sendPedal(unsigned pedal, unsigned val);
    
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void keyLEDChanged(OSCCommunication*, int course, int key, int colour) = 0;
    };
    void addListener(Listener *listenerToAdd);
    void removeListener(Listener *listenerToRemove);
    
private:
    juce::OSCSender sender;
    juce::String senderIP;
    int senderPort = -1;
    
    juce::OSCReceiver receiver;
    juce::String receiverIP;
    int receiverPort = -1;
    
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void timerCallback() override;
    
    juce::ListenerList<Listener> listeners;
    void sendkeyLEDEventMessage(int course, int key, int colour);
};
