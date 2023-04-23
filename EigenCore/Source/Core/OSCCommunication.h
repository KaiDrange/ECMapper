#pragma once

#include <JuceHeader.h>
#include <iostream>
#include <thread>
#include "OSCMessageQueue.h"
#include "Common.h"

#define MSGPROCESS_MICROSEC_SLEEP 100
//#define MEASURE_OSCSENDPROCESSTIME

struct ConnectedDevice;

class OSCCommunication : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>, juce::Timer {
public:
    OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue);
    ~OSCCommunication();
    bool connectSender(juce::String ip, int port);
    void disconnectSender();
    bool connectReceiver(int port);
    void disconnectReceiver();
    
    void sendKey(unsigned course, unsigned key, bool a, unsigned p, int r, int y, EHDeviceType deviceType);
    void sendDevice(EHDeviceType deviceType);
    void sendBreath(unsigned val, EHDeviceType deviceType);
    void sendStrip(unsigned strip, unsigned val, bool active, EHDeviceType deviceType);
    void sendPedal(unsigned pedal, unsigned val, EHDeviceType deviceType);

    OSC::OSCMessageFifo *sendQueue;
private:
    juce::OSCSender sender;
    juce::String senderIP;
    int senderPort = -1;
    bool senderConnected = false;
    
    juce::OSCReceiver receiver;
    juce::String receiverIP;
    int receiverPort = -1;
    bool receiverConnected = false;
    
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void timerCallback() override;
    
    int pingCounter = -1;
    int pingInterval = 100;
    
    OSC::OSCMessageFifo *receiveQueue;
    OSC::Message msg;
    
    void* sendProcess();
    std::thread sendProcessThread;
};

#include "APICallback.h"
