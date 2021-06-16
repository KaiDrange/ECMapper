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
    senderIsConnected = false;
    receiverIsConnected = false;
}

bool OSCCommunication::connectSender() {
    senderIsConnected = sender.connect(senderIP, senderPort);
    return senderIsConnected;
}

void OSCCommunication::disconnectSender() {
    sender.disconnect();
    senderIsConnected = false;
}

bool OSCCommunication::connectReceiver() {
    receiverIsConnected = receiver.connect(receiverPort);
    return receiverIsConnected;
}

void OSCCommunication::disconnectReceiver() {
    receiver.disconnect();
    receiverIsConnected = false;
}

void OSCCommunication::oscMessageReceived(const juce::OSCMessage &message) {
    if (message.getAddressPattern() == "/EigenCore/ping") {
        if (pingCounter == -1) {
            std::cout << "Core connected" << std::endl;
            eigenCoreConnected = true;
        }
        pingCounter = 0;
//        receiveQueue->add(&msg);

    }
    else if (message.getAddressPattern() == "/EigenCore/key" && message.size() == 7) {
        msg = {
            .type = OSC::MessageType::Key,
            .course = (unsigned int)message[0].getInt32(),
            .key = (unsigned int)message[1].getInt32(),
            .active = message[2].getInt32(),
            .pressure = (unsigned int)message[3].getInt32(),
            .roll = message[4].getInt32(),
            .yaw = message[5].getInt32(),
            .strip = 0,
            .pedal = 0,
            .value = 0,
            .device = (DeviceType)message[6].getInt32()
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/EigenCore/breath" && message.size() == 2) {
        msg = {
            .type = OSC::MessageType::Breath,
            .course = 0,
            .key = 0,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = 0,
            .pedal = 0,
            .value = (unsigned int)message[0].getInt32(),
            .device = (DeviceType)message[1].getInt32()
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/EigenCore/strip" && message.size() == 4) {
        msg = {
            .type = OSC::MessageType::Strip,
            .course = 0,
            .key = 0,
            .active = message[2].getInt32(),
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = (unsigned int)message[0].getInt32(),
            .pedal = 0,
            .value = (unsigned int)message[1].getInt32(),
            .device = (DeviceType)message[3].getInt32()
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/EigenCore/pedal" && message.size() == 3) {
        msg = {
            .type = OSC::MessageType::Pedal,
            .course = 0,
            .key = 0,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = 0,
            .pedal = (unsigned int)message[0].getInt32(),
            .value = (unsigned int)message[1].getInt32(),
            .device = (DeviceType)message[2].getInt32()
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/EigenCore/device" && message.size() == 1) {
        msg = {
            .type = OSC::MessageType::Device,
            .course = 0,
            .key = 0,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = 0,
            .pedal = 0,
            .value = 0,
            .device = (DeviceType)message[0].getInt32()
        };
    }
}

void OSCCommunication::sendLED(int course, int key, int led, DeviceType deviceType) {
    if (!senderIsConnected)
        return;
    
    sender.send("/ECMapper/led", course, key, led, (int)deviceType);
}

void OSCCommunication::sendReset(DeviceType deviceType) {
    if (!senderIsConnected)
        return;
    
    sender.send("/ECMapper/reset",(int)deviceType);
}

void OSCCommunication::timerCallback() {
    if (!senderIsConnected)
        return;
    
    sender.send("/ECMapper/ping");
    if (pingCounter > -1)
        pingCounter++;
    
    if (pingCounter > 10) {
        std::cout << "Connection to Core timed out" << std::endl;
        pingCounter = -1;
        eigenCoreConnected = false;
    }
    
    sendOutgoingMessages();
}

void OSCCommunication::sendOutgoingMessages() {
    if (!senderIsConnected)
        return;
    
    static OSC::Message msg;
    while (sendQueue->getMessageCount() > 0) {
        sendQueue->read(&msg);
        switch (msg.type) {
            case OSC::MessageType::LED:
                sendLED(msg.course, msg.key, msg.value, msg.device);
                break;
            case OSC::MessageType::Reset:
                sendReset(msg.device);
                break;
            default:
                break;
        }
    }
}
