#include "OSCCommunication.h"

OSCCommunication::OSCCommunication() {
    receiver.addListener(this);
    receiver.registerFormatErrorHandler([this](const char *data, int dataSize) {
        std::cout << "invalid OSC data";
    });
    
    startTimer(1000);
}

OSCCommunication::~OSCCommunication() {
    stopTimer();
    sender.disconnect();
    receiver.disconnect();
}

bool OSCCommunication::connectSender(juce::String ip, int port)  {
    this->senderIP = ip;
    this->senderPort = port;
    return sender.connect(senderIP, senderPort);
}

void OSCCommunication::disconnectSender() {
    sender.disconnect();
    senderPort = -1;
}

bool OSCCommunication::connectReceiver(int port)  {
    this->receiverPort = port;
    return receiver.connect(receiverPort);
}

void OSCCommunication::disconnectReceiver() {
    receiver.disconnect();
    receiverPort = -1;
}

void OSCCommunication::oscMessageReceived(const juce::OSCMessage &message) {
    std::cout << "Message received";
}

void OSCCommunication::sendLED(int course, int key, int led) {
    sender.send("/EigenharpMapper/led", course, key, led);
}


void OSCCommunication::timerCallback() {
    sender.send("/EigenharpMapper/ping");
    sendLED(0, 4, 2);
}
