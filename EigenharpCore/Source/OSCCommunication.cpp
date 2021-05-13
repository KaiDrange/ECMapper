#include "OSCCommunication.h"

OSCCommunication::OSCCommunication(OSC::OSCMessageFifo *sendQueue, OSC::OSCMessageFifo *receiveQueue) {

    this->sendQueue = sendQueue;
    this->receiveQueue = receiveQueue;
    receiver.addListener(this);
    receiver.registerFormatErrorHandler([this](const char *data, int dataSize) {
        std::cout << "invalid OSC data";
    });
    
    startTimer(pingInterval);
    
    sendProcessThread = std::thread(&OSCCommunication::sendProcess, this);
}

OSCCommunication::~OSCCommunication() {
    sendProcessThread.join();
    
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
    if (message.getAddressPattern() == "/EigenharpMapper/led" && message.size() == 3) {
        msg = {
            .type = OSC::MessageType::LED,
            .course = (unsigned int)message[0].getInt32(),
            .key = (unsigned int)message[1].getInt32(),
            .colour = message[2].getInt32(),
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = 0,
            .pedal = 0
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/EigenharpMapper/ping") {
        if (pingCounter == -1)
            std::cout << "Mapper connected" << std::endl;
        pingCounter = 0;
    }
}

void OSCCommunication::sendDeviceName(const juce::String &name) {
    if (pingCounter > -1)
        sender.send("/EigenharpCore/device", name);
}

void OSCCommunication::sendKey(unsigned course, unsigned key, bool a, unsigned p, int r, int y) {
    if (pingCounter > -1)
        sender.send("/EigenharpCore/key", (int)course, (int)key, (int)a, (int)p, (int)r, (int)y);
}

void OSCCommunication::sendBreath(unsigned val) {
    if (pingCounter > -1)
        sender.send("/EigenharpCore/breath", (int)val);
}

void OSCCommunication::sendStrip(unsigned strip, unsigned val) {
    if (pingCounter > -1)
        sender.send("/EigenharpCore/strip", (int)strip, (int)val);
}

void OSCCommunication::sendPedal(unsigned pedal, unsigned val) {
    if (pingCounter > -1)
        sender.send("/EigenharpCore/pedal", (int)pedal, (int)val);
}

void OSCCommunication::timerCallback() {
    sender.send("/EigenharpCore/ping");
    if (pingCounter > -1)
        pingCounter++;
    
    if (pingCounter > 10) {
        std::cout << "Connection to Mapper timed out" << std::endl;
        pingCounter = -1;
    }
}

void* OSCCommunication::sendProcess() {
    while (!exitThreads) {
#ifdef MEASURE_OSCSENDPROCESSTIME
        static int counter = 0;
        auto begin = std::chrono::high_resolution_clock::now();
#endif
        static OSC::Message msg;
        while (sendQueue->getMessageCount() > 0) {
            sendQueue->read(&msg);
            switch (msg.type) {
                case OSC::MessageType::Key:
                    sendKey(msg.course, msg.key, msg.active, msg.pressure, msg.roll, msg.yaw);
                    break;
                default:
                    break;
            }
        }
#ifdef MEASURE_OSCSENDPROCESSTIME
        auto end = std::chrono::high_resolution_clock::now();
        if (counter%1000 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin);
            std::cout << "OSC sendProcess time: " << elapsed.count() << std::endl;
        }
#endif

        std::this_thread::sleep_for(std::chrono::microseconds(MSGPROCESS_MICROSEC_SLEEP));
    }
    return nullptr;
}
