#include "OSCCommunication.h"

static int receiverListenerCount = 0;
static juce::OSCReceiver *receiver = nullptr;
static bool receiverIsConnected = false;

OSCCommunication::OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue, Logger *logger) {
    this->logger = logger;
    if (receiver == nullptr)
        receiver = new juce::OSCReceiver();
    this->sendQueue = sendQueue;
    this->receiveQueue = receiveQueue;
    receiver->registerFormatErrorHandler([this](const char *data, int dataSize) {
        std::cout << "invalid OSC data" << std::endl;
    });
    
    startTimer(pingInterval);
}

OSCCommunication::~OSCCommunication() {
    stopTimer();
    disconnectSender();
    disconnectReceiver();
    if (receiverListenerCount == 0 && receiver != nullptr) {
        delete receiver;
        receiver = nullptr;
    }
}

bool OSCCommunication::connectSender() {
    senderIsConnected = sender.connect(senderIP, senderPort);
    logger->log("ConnectSender called: senderIsConnected is now: " + juce::String((int)senderIsConnected));
    return senderIsConnected;
}

void OSCCommunication::disconnectSender() {
    sender.disconnect();
    senderIsConnected = false;
    logger->log("disconnectSender called: senderIsConnected is now: " + juce::String((int)senderIsConnected));
}

bool OSCCommunication::connectReceiver() {
    if (!receiverIsConnected)
        receiverIsConnected = receiver->connect(receiverPort);
    if (!isListeningToReceiver && receiverIsConnected) {
        receiver->addListener(this);
        receiverListenerCount++;
        isListeningToReceiver = true;
    }
    logger->log("ConnectReceiver called: receiverIsConnected is now: " + juce::String((int)receiverIsConnected));
    logger->log("ReceiverListening count is now: " + juce::String(receiverListenerCount));
    return receiverIsConnected;
}

void OSCCommunication::disconnectReceiver() {
    if (isListeningToReceiver) {
        receiver->removeListener(this);
        receiverListenerCount--;
        isListeningToReceiver = false;
    }
    if (receiverListenerCount == 0) {
        receiver->disconnect();
        receiverIsConnected = false;
    }
    logger->log("DisconnectReceiver called: receiverIsConnected is now: " + juce::String((int)receiverIsConnected));
    logger->log("ReceiverListening count is now: " + juce::String(receiverListenerCount));
}

void OSCCommunication::oscMessageReceived(const juce::OSCMessage &message) {
    if (message.getAddressPattern() == "/EigenCore/ping") {
        if (pingCounter == -1) {
            logger->log("Core connected.");
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
    if (senderIsConnected)
        sender.send("/ECMapper/ping");
    if (pingCounter > -1)
        pingCounter++;
    
    if (pingCounter > 10) {
        logger->log("Connection to Core timed out");
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
