#include "OSCCommunication.h"

OSCCommunication::OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue) {
    receiver.addListener(this);
    this->sendQueue = sendQueue;
    this->receiveQueue = receiveQueue;
    receiver.registerFormatErrorHandler([this](const char *data, int dataSize) {
        std::cout << "invalid OSC data" << std::endl;
    });
    
    startTimer(pingInterval);
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
    if (message.getAddressPattern() == "/EigenharpCore/ping") {
        if (pingCounter == -1)
            std::cout << "Core connected" << std::endl;
        pingCounter = 0;
    }
    else if (message.getAddressPattern() == "/EigenharpCore/key") {
        
    }
}

void OSCCommunication::sendLED(int course, int key, int led) {
    if (pingCounter > -1)
        sender.send("/EigenharpMapper/led", course, key, led);
}

void OSCCommunication::timerCallback() {
    sender.send("/EigenharpMapper/ping");
    if (pingCounter > -1)
        pingCounter++;
    
    if (pingCounter > 10) {
        std::cout << "Connection to Core timed out" << std::endl;
        pingCounter = -1;
    }
    
    sendOutgoingMessages();
}

void OSCCommunication::sendOutgoingMessages() {
    static OSC::Message msg;
    while (sendQueue->getMessageCount() > 0) {
        sendQueue->read(&msg);
        switch (msg.type) {
            case OSC::MessageType::LED:
                sendLED(msg.course, msg.key, msg.value);
                break;
            default:
                break;
        }
    }
}
