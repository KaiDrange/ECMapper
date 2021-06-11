#pragma once

#include <JuceHeader.h>
#include <iostream>
#include "OSCMessageQueue.h"
#include "../Models/Enums.h"

class OSCCommunication : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, juce::Timer {
public:
    OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue);
    ~OSCCommunication();
    bool connectSender(juce::String ip, int port);
    bool connectSender();
    void disconnectSender();
    bool connectReceiver(int port);
    bool connectReceiver();
    void disconnectReceiver();
    bool receiverIsConnected = false;
    bool senderIsConnected = false;
    
    void sendLED(int course, int key, int led, DeviceType deviceType);
    OSC::OSCMessageFifo *receiveQueue;

private:
    juce::OSCSender sender;
    juce::String senderIP;
    int senderPort = -1;
    
    juce::OSCReceiver receiver;
    juce::String receiverIP;
    int receiverPort = -1;
    
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void timerCallback() override;
    int pingCounter = -1;
    const int pingInterval = 100;

    OSC::OSCMessageFifo *sendQueue;
    OSC::Message msg;
    
    void sendOutgoingMessages();
};
