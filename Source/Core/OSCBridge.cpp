#include "OSCBridge.h"

namespace ecm {

OSCBridge::OSCBridge(osc::MessageFifo& hardwareToMapperQueue, 
                    osc::MessageFifo& mapperToHardwareQueue, 
                    osc::MessageFifo& outgoingOSCQueue,
                    Logger& logger)
    : hardwareToMapperQueue_(hardwareToMapperQueue),
      mapperToHardwareQueue_(mapperToHardwareQueue),
      outgoingOSCQueue_(outgoingOSCQueue),
      logger_(logger) {
}

OSCBridge::~OSCBridge() {
    stopTimer();
    setSenderEnabled(false);
    setReceiverEnabled(false);
}

void OSCBridge::setSenderEnabled(bool enabled) {
    if (senderEnabled_ == enabled) return;
    senderEnabled_ = enabled;
    if (senderEnabled_) {
        senderConnected_ = sender_.connect(senderIP_, senderPort_);
        logger_.log("OSC Sender " + juce::String(senderConnected_ ? "connected" : "failed") + " to " + senderIP_ + ":" + juce::String(senderPort_));
        if (senderConnected_) {
            if (!isTimerRunning()) startTimer(100);
        }
    } else {
        sender_.disconnect();
        senderConnected_ = false;
        if (!receiverEnabled_) stopTimer();
    }
}

void OSCBridge::setReceiverEnabled(bool enabled) {
    if (receiverEnabled_ == enabled) return;
    receiverEnabled_ = enabled;
    if (receiverEnabled_) {
        receiverConnected_ = receiver_.connect(receiverPort_);
        if (receiverConnected_) {
            receiver_.addListener(this);
            logger_.log("OSC Receiver listening on port " + juce::String(receiverPort_));
            if (!isTimerRunning()) startTimer(100);
        }
    } else {
        receiver_.removeListener(this);
        receiver_.disconnect();
        receiverConnected_ = false;
        if (!senderEnabled_) stopTimer();
    }
}

void OSCBridge::setSenderTarget(const juce::String& ip, int port) {
    senderIP_ = ip;
    senderPort_ = port;
    if (senderEnabled_) {
        setSenderEnabled(false);
        setSenderEnabled(true);
    }
}

void OSCBridge::setReceiverPort(int port) {
    receiverPort_ = port;
    if (receiverEnabled_) {
        setReceiverEnabled(false);
        setReceiverEnabled(true);
    }
}

void OSCBridge::timerCallback() {
    if (senderConnected_) {
        sendPing();
        sendOutgoingMessages();
    }
}

void OSCBridge::sendPing() {
    sender_.send("/ECMapper/ping");
}

void OSCBridge::sendOutgoingMessages() {
    osc::Message msg;
    while (outgoingOSCQueue_.read(msg)) {
        switch (msg.type) {
            case osc::MessageType::Key:
                sender_.send("/EigenCore/key", (int)msg.course, (int)msg.key, msg.active, (int)msg.pressure, msg.roll, msg.yaw, (int)msg.device);
                break;
            case osc::MessageType::Breath:
                sender_.send("/EigenCore/breath", (int)msg.value, (int)msg.device);
                break;
            case osc::MessageType::Strip:
                sender_.send("/EigenCore/strip", (int)msg.strip, (int)msg.value, msg.active, (int)msg.device);
                break;
            case osc::MessageType::Pedal:
                sender_.send("/EigenCore/pedal", (int)msg.pedal, (int)msg.value, (int)msg.device);
                break;
            case osc::MessageType::Device:
                sender_.send("/EigenCore/device", (int)msg.device);
                break;
            case osc::MessageType::LED:
                sender_.send("/ECMapper/led", (int)msg.course, (int)msg.key, (int)msg.value, (int)msg.device);
                break;
            case osc::MessageType::Reset:
                sender_.send("/ECMapper/reset", (int)msg.device);
                break;
            default: break;
        }
    }
}

void OSCBridge::oscMessageReceived(const juce::OSCMessage& message) {
    osc::Message msg;
    auto pattern = message.getAddressPattern().toString();
    
    if (pattern == "/EigenCore/key" && message.size() == 7) {
        msg.type = osc::MessageType::Key;
        msg.course = (unsigned int)message[0].getInt32();
        msg.key = (unsigned int)message[1].getInt32();
        msg.active = message[2].getInt32();
        msg.pressure = (unsigned int)message[3].getInt32();
        msg.roll = message[4].getInt32();
        msg.yaw = message[5].getInt32();
        msg.device = (InstrumentType)message[6].getInt32();
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/EigenCore/breath" && message.size() == 2) {
        msg.type = osc::MessageType::Breath;
        msg.value = (unsigned int)message[0].getInt32();
        msg.device = (InstrumentType)message[1].getInt32();
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/EigenCore/strip" && message.size() == 4) {
        msg.type = osc::MessageType::Strip;
        msg.strip = (unsigned int)message[0].getInt32();
        msg.value = (unsigned int)message[1].getInt32();
        msg.active = message[2].getInt32();
        msg.device = (InstrumentType)message[3].getInt32();
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/EigenCore/pedal" && message.size() == 3) {
        msg.type = osc::MessageType::Pedal;
        msg.pedal = (unsigned int)message[0].getInt32();
        msg.value = (unsigned int)message[1].getInt32();
        msg.device = (InstrumentType)message[2].getInt32();
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/ECMapper/led" && message.size() == 4) {
        msg.type = osc::MessageType::LED;
        msg.course = (unsigned int)message[0].getInt32();
        msg.key = (unsigned int)message[1].getInt32();
        msg.value = (unsigned int)message[2].getInt32();
        msg.device = (InstrumentType)message[3].getInt32();
        mapperToHardwareQueue_.add(msg);
    } else if (pattern == "/ECMapper/reset" && message.size() == 1) {
        msg.type = osc::MessageType::Reset;
        msg.device = (InstrumentType)message[0].getInt32();
        mapperToHardwareQueue_.add(msg);
    }
}

} // namespace ecm
