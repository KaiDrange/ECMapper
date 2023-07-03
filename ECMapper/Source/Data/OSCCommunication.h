#pragma once

#include <JuceHeader.h>
#include <iostream>
#include "OSCMessageQueue.h"
#include "../Models/Enums.h"

class OSCCommunication : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, juce::Timer {
public:
    OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue);
    ~OSCCommunication();
    bool connectSender();
    void disconnectSender();
    bool connectReceiver();
    void disconnectReceiver();
    bool senderIsConnected = false;
    bool isListeningToReceiver = false;
    bool eigenCoreConnected = false;
    
    void sendLED(int course, int key, int led, DeviceType deviceType);
    void sendReset(DeviceType deviceType);
    OSC::OSCMessageFifo *receiveQueue;
    juce::String senderIP;
    int senderPort = -1;
    int receiverPort = -1;

private:
    juce::OSCSender sender;
        
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void timerCallback() override;
    int pingCounter = -1;
    const int pingInterval = 100;

    OSC::OSCMessageFifo *sendQueue;
    OSC::Message msg;
    
    void sendOutgoingMessages();
};
