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
    std::cout << "Connecting to: " << senderIP << std::endl;
    std::cout << "Transmitting on port: " << senderPort << std::endl;
    senderConnected = true;
    return sender.connect(senderIP, senderPort);
}

void OSCCommunication::disconnectSender() {
    sender.disconnect();
    senderConnected = false;
    senderPort = -1;
}

bool OSCCommunication::connectReceiver(int port)  {
    this->receiverPort = port;
    receiverConnected = true;
    std::cout << "Receiving on port: " << receiverPort << std::endl;
    return receiver.connect(receiverPort);
}

void OSCCommunication::disconnectReceiver() {
    receiver.disconnect();
    receiverPort = -1;
    receiverConnected = false;
}

void OSCCommunication::oscMessageReceived(const juce::OSCMessage &message) {
    if (message.getAddressPattern() == "/ECMapper/led" && message.size() == 4) {
        msg = {
            .type = OSC::MessageType::LED,
            .course = (unsigned int)message[0].getInt32(),
            .key = (unsigned int)message[1].getInt32(),
            .value = (unsigned int)message[2].getInt32(),
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = 0,
            .pedal = 0,
            .device = (EHDeviceType)message[3].getInt32()
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/ECMapper/reset" && message.size() == 1) {
        msg = {
            .type = OSC::MessageType::Reset,
            .course = 0,
            .key = 0,
            .value = 0,
            .active = 0,
            .pressure = 0,
            .roll = 0,
            .yaw = 0,
            .strip = 0,
            .pedal = 0,
            .device = (EHDeviceType)message[0].getInt32()
        };

        receiveQueue->add(&msg);
    }
    else if (message.getAddressPattern() == "/ECMapper/ping") {
        if (pingCounter == -1) {
            mapperConnected = true;
            std::cout << "Mapper connected" << std::endl;
            pingCounter = 0;
            for (auto i = begin(connectedDevices); i != end(connectedDevices); i++) {
                sendDevice(i->type);
            }
        }
        pingCounter = 0;
    }
}

void OSCCommunication::sendDevice(const EHDeviceType deviceType) {
    if (pingCounter > -1)
        sender.send("/EigenCore/device", (int)deviceType);
}

void OSCCommunication::sendKey(unsigned course, unsigned key, bool a, unsigned p, int r, int y, EHDeviceType deviceType) {
    if (pingCounter > -1)
        sender.send("/EigenCore/key", (int)course, (int)key, (int)a, (int)p, (int)r, (int)y, (int)deviceType);
}

void OSCCommunication::sendBreath(unsigned val, EHDeviceType deviceType) {
    if (pingCounter > -1)
        sender.send("/EigenCore/breath", (int)val, (int)deviceType);
}

void OSCCommunication::sendStrip(unsigned strip, unsigned val, bool active, EHDeviceType deviceType) {
    if (pingCounter > -1)
        sender.send("/EigenCore/strip", (int)strip, (int)val, active, (int)deviceType);
}

void OSCCommunication::sendPedal(unsigned pedal, unsigned val, EHDeviceType deviceType) {
    if (pingCounter > -1)
        sender.send("/EigenCore/pedal", (int)pedal, (int)val, (int)deviceType);
}

void OSCCommunication::timerCallback() {
    if (!senderConnected)
        return;
    
    sender.send("/EigenCore/ping");
    if (pingCounter > -1)
        pingCounter++;
    
    if (pingCounter > 15) {
        mapperConnected = false;
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
            if (!senderConnected)
                continue;

            switch (msg.type) {
                case OSC::MessageType::Key:
                    sendKey(msg.course, msg.key, msg.active, msg.pressure, msg.roll, msg.yaw, msg.device);
                    break;
                case OSC::MessageType::Breath:
                    sendBreath(msg.value, msg.device);
                    break;
                case OSC::MessageType::Strip:
                    sendStrip(msg.strip, msg.value, msg.active, msg.device);
                    break;
                case OSC::MessageType::Pedal:
                    sendPedal(msg.pedal, msg.value, msg.device);
                    break;
                case OSC::MessageType::Device:
                    sendDevice(msg.device);
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

        std::this_thread::sleep_for(std::chrono::microseconds(MSGPROCESS_MICROSEC_SLEEP + 100000*!mapperConnected));
    }
    return nullptr;
}
