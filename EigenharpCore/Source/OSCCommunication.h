#pragma once

#include <JuceHeader.h>
#include <iostream>
#include <thread>
#include "OSCMessageQueue.h"

#define MSGPROCESS_MICROSEC_SLEEP 10

class OSCCommunication : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, juce::Timer {
public:
    OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue);
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

    OSC::OSCMessageFifo *sendQueue;
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
    int pingInterval = 100;
    
    OSC::OSCMessageFifo *receiveQueue;
    OSC::Message msg;
    
    void* sendProcess();
    std::thread sendProcessThread;
};
