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
                sender_.send("/EigenCore/key", (int)msg.course, (int)msg.key, (int)msg.active, msg.pressure, msg.roll, msg.yaw, (int)msg.device);
                break;
            case osc::MessageType::Breath:
                sender_.send("/EigenCore/breath", msg.value, (int)msg.device);
                break;
            case osc::MessageType::Strip:
                sender_.send("/EigenCore/strip", (int)msg.strip, msg.value, (int)msg.active, (int)msg.device);
                break;
            case osc::MessageType::Pedal:
                sender_.send("/EigenCore/pedal", (int)msg.pedal, msg.value, (int)msg.device);
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
    
    auto getInt = [](const juce::OSCArgument& v) { return v.isInt32() ? v.getInt32() : (int)v.getFloat32(); };
    auto getFloat = [](const juce::OSCArgument& v) { return v.isFloat32() ? v.getFloat32() : (float)v.getInt32(); };

    if (pattern == "/EigenCore/key" && message.size() == 7) {
        msg.type = osc::MessageType::Key;
        msg.course = (unsigned int)getInt(message[0]);
        msg.key = (unsigned int)getInt(message[1]);
        msg.active = getInt(message[2]);
        msg.pressure = getFloat(message[3]);
        msg.roll = getFloat(message[4]);
        msg.yaw = getFloat(message[5]);
        msg.device = (InstrumentType)getInt(message[6]);
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/EigenCore/breath" && message.size() == 2) {
        msg.type = osc::MessageType::Breath;
        msg.value = getFloat(message[0]);
        msg.device = (InstrumentType)getInt(message[1]);
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/EigenCore/strip" && message.size() == 4) {
        msg.type = osc::MessageType::Strip;
        msg.strip = (unsigned int)getInt(message[0]);
        msg.value = getFloat(message[1]);
        msg.active = getInt(message[2]);
        msg.device = (InstrumentType)getInt(message[3]);
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/EigenCore/pedal" && message.size() == 3) {
        msg.type = osc::MessageType::Pedal;
        msg.pedal = (unsigned int)getInt(message[0]);
        msg.value = getFloat(message[1]);
        msg.device = (InstrumentType)getInt(message[2]);
        hardwareToMapperQueue_.add(msg);
    } else if (pattern == "/ECMapper/led" && message.size() == 4) {
        msg.type = osc::MessageType::LED;
        msg.course = (unsigned int)getInt(message[0]);
        msg.key = (unsigned int)getInt(message[1]);
        msg.value = (unsigned int)getInt(message[2]);
        msg.device = (InstrumentType)getInt(message[3]);
        mapperToHardwareQueue_.add(msg);
    } else if (pattern == "/ECMapper/reset" && message.size() == 1) {
        msg.type = osc::MessageType::Reset;
        msg.device = (InstrumentType)getInt(message[0]);
        mapperToHardwareQueue_.add(msg);
    }
}

} // namespace ecm
