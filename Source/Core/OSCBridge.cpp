#include "OSCBridge.h"

namespace ecm {

OSCBridge::OSCBridge(HardwareService& hardwareService,
                    osc::MessageFifo& hardwareToMapperQueue, 
                    osc::MessageFifo& mapperToHardwareQueue, 
                    osc::MessageFifo& outgoingOSCQueue,
                    ecm::Logger& logger)
    : hardwareService_(hardwareService),
      hardwareToMapperQueue_(hardwareToMapperQueue),
      mapperToHardwareQueue_(mapperToHardwareQueue),
      outgoingOSCQueue_(outgoingOSCQueue),
      logger_(logger) {
    instanceId_ = juce::Uuid().toString();
    hardwareService_.addListener(this);
    discoverySender_.connect("127.0.0.1", 12121);
}

OSCBridge::~OSCBridge() {
    hardwareService_.removeListener(this);
    stopTimer();
    const juce::ScopedLock sl(connectionsLock_);
    connections_.clear();
}

void OSCBridge::deviceListChanged() {
    updateConnections();
    updateSlaveReceiver();
}

void OSCBridge::updateSlaveReceiver() {
    if (hardwareService_.getAppRole() == AppRole::Slave) {
        int port = hardwareService_.getSlaveListenPort();
        if (globalSlaveReceiver_ == nullptr || !globalSlaveReceiver_->connect(port)) {
            globalSlaveReceiver_ = std::make_unique<juce::OSCReceiver>();
            if (globalSlaveReceiver_->connect(port)) {
                globalSlaveReceiver_->addListener(this);
                logger_.log("Global Slave Receiver listening on port " + juce::String(port));
            } else {
                logger_.log("Global Slave Receiver FAILED to listen on port " + juce::String(port));
            }
        }
    } else {
        if (globalSlaveReceiver_ != nullptr) {
            globalSlaveReceiver_->removeListener(this);
            globalSlaveReceiver_->disconnect();
            globalSlaveReceiver_ = nullptr;
        }
    }
}

void OSCBridge::updateConnections() {
    const juce::ScopedLock sl(connectionsLock_);
    connections_.clear();
    
    if (!masterEnabled_) {
        stopTimer();
        return;
    }
    
    auto devices = hardwareService_.getConnectedDevices();
    for (const auto& d : devices) {
        if (d.mode == ecm::DeviceMode::Local) continue;
        
        auto conn = std::make_unique<Connection>();
        conn->dev = d.dev;
        conn->type = d.type;
        conn->mode = d.mode;
        conn->ip = d.oscIP;
        
        if (d.mode == ecm::DeviceMode::TransmitOSC) {
            conn->sendPort = d.oscPort;
            conn->receivePort = d.oscPort + 1;
        } else {
            conn->sendPort = d.oscPort + 1;
            conn->receivePort = d.oscPort;
        }
        
        conn->sender = std::make_unique<juce::OSCSender>();
        if (conn->sender->connect(conn->ip, conn->sendPort)) {
            logger_.log("OSC Sender connected to " + conn->ip + ":" + juce::String(conn->sendPort) + " for " + d.dev);
        } else {
            logger_.log("OSC Sender FAILED to connect to " + conn->ip + ":" + juce::String(conn->sendPort) + " for " + d.dev);
        }
        
        bool needsReceiver = true;
        if (globalSlaveReceiver_ != nullptr && conn->receivePort == hardwareService_.getSlaveListenPort()) {
            needsReceiver = false;
        }
        
        if (needsReceiver) {
            conn->receiver = std::make_unique<juce::OSCReceiver>();
            if (conn->receiver->connect(conn->receivePort)) {
                conn->receiver->addListener(this);
                logger_.log("OSC Receiver listening on port " + juce::String(conn->receivePort) + " for " + d.dev);
            } else {
                logger_.log("OSC Receiver FAILED to listen on port " + juce::String(conn->receivePort) + " for " + d.dev);
            }
        } else {
            logger_.log("OSC Receiver using global slave receiver for " + d.dev);
        }
        
        connections_.push_back(std::move(conn));
    }
    
    if (masterEnabled_) {
        if (!isTimerRunning()) startTimer(100);
    } else {
        stopTimer();
    }
}

void OSCBridge::setSenderEnabled(bool enabled) {
    if (masterEnabled_ == enabled) return;
    masterEnabled_ = enabled;
    updateConnections();
}

void OSCBridge::setReceiverEnabled(bool enabled) {
    if (enabled) {
        if (discoveryReceiver_.connect(12121)) {
            discoveryReceiver_.addListener(this);
            logger_.log("Discovery OSC Receiver listening on port 12121");
        }
    } else {
        discoveryReceiver_.removeListener(this);
        discoveryReceiver_.disconnect();
    }
    setSenderEnabled(enabled);
}

void OSCBridge::setSenderTarget(const juce::String& ip, int port) {
    // Old global method, ignore or use as default?
    // For now we rely on per-device settings.
}

void OSCBridge::setReceiverPort(int port) {
    // Old global method, ignore.
}

void OSCBridge::timerCallback() {
    const juce::ScopedLock sl(connectionsLock_);
    for (auto& conn : connections_) {
        sendPing(conn.get());
    }
    
    // Periodic discovery broadcast (every 2 seconds)
    static int discoveryCounter = 0;
    if (++discoveryCounter >= 20) {
        discoveryCounter = 0;
        auto devices = hardwareService_.getConnectedDevices();
        for (const auto& d : devices) {
            if (!d.isRemote) {
                juce::String localIP = juce::IPAddress::getLocalAddress().toString();
                
                // Send to global discovery port
                discoverySender_.send("/EigenCore/device", (int)d.type, localIP, d.oscPort, instanceId_, juce::String(d.dev));

                // Also send to all active Transmit connections
                for (auto& conn : connections_) {
                    if (conn->mode == ecm::DeviceMode::TransmitOSC) {
                        conn->sender->send("/EigenCore/device", (int)d.type, localIP, d.oscPort, instanceId_, juce::String(d.dev));
                    }
                }
            }
        }
    }
    
    sendOutgoingMessages();
}

void OSCBridge::sendPing(Connection* conn) {
    if (conn && conn->sender) {
        conn->sender->send("/ECMapper/ping");
    }
}

void OSCBridge::sendOutgoingMessages() {
    osc::Message msg;
    while (outgoingOSCQueue_.read(msg)) {
        if (msg.type == osc::MessageType::Device) {
            int port = 12130;
            auto devices = hardwareService_.getConnectedDevices();
            for (const auto& d : devices) {
                if (d.dev == msg.devId) {
                    port = d.oscPort;
                    break;
                }
            }
            discoverySender_.send("/EigenCore/device", (int)msg.device, juce::IPAddress::getLocalAddress().toString(), port, instanceId_, juce::String(msg.devId)); 
        }
        
        const juce::ScopedLock sl(connectionsLock_);
        for (auto& conn : connections_) {
            bool isPerformanceMsg = (msg.type == osc::MessageType::Key || 
                                     msg.type == osc::MessageType::Breath || 
                                     msg.type == osc::MessageType::Strip || 
                                     msg.type == osc::MessageType::Pedal || 
                                     msg.type == osc::MessageType::Device);
            
            bool shouldSend = false;
            if (conn->type == msg.device) {
                if (isPerformanceMsg && conn->mode == ecm::DeviceMode::TransmitOSC) shouldSend = true;
                else if (!isPerformanceMsg && conn->mode == ecm::DeviceMode::ReceiveOSC) shouldSend = true;
            }

            if (shouldSend) {
                if (std::strlen(msg.devId) > 0 && conn->dev != msg.devId) continue;

                switch (msg.type) {
                    case osc::MessageType::Key:
                        conn->sender->send("/EigenCore/key", (int)msg.course, (int)msg.key, (int)msg.active, msg.pressure, msg.roll, msg.yaw, (int)msg.device, juce::String(msg.devId));
                        break;
                    case osc::MessageType::Breath:
                        conn->sender->send("/EigenCore/breath", msg.value, (int)msg.device, juce::String(msg.devId));
                        break;
                    case osc::MessageType::Strip:
                        conn->sender->send("/EigenCore/strip", (int)msg.strip, msg.value, (int)msg.active, (int)msg.device, juce::String(msg.devId));
                        break;
                    case osc::MessageType::Pedal:
                        conn->sender->send("/EigenCore/pedal", (int)msg.pedal, msg.value, (int)msg.device, juce::String(msg.devId));
                        break;
                    case osc::MessageType::Device:
                        conn->sender->send("/EigenCore/device", (int)msg.device, juce::IPAddress::getLocalAddress().toString(), conn->sendPort, instanceId_, juce::String(msg.devId));
                        break;
                    case osc::MessageType::LED:
                        conn->sender->send("/ECMapper/led", (int)msg.course, (int)msg.key, (int)msg.value, (int)msg.device, juce::String(msg.devId));
                        break;
                    case osc::MessageType::Reset:
                        conn->sender->send("/ECMapper/reset", (int)msg.device, juce::String(msg.devId));
                        break;
                    default: break;
                }
            }
        }
    }
}

void OSCBridge::oscMessageReceived(const juce::OSCMessage& message) {
    osc::Message msg;
    auto pattern = message.getAddressPattern().toString();
    
    auto getInt = [](const juce::OSCArgument& v) { return v.isInt32() ? v.getInt32() : (int)v.getFloat32(); };
    auto getFloat = [](const juce::OSCArgument& v) { return v.isFloat32() ? v.getFloat32() : (float)v.getInt32(); };

    if (pattern == "/EigenCore/device") {
        if (message.size() >= 5) {
            auto devType = (InstrumentType)getInt(message[0]);
            auto remoteIP = message[1].getString();
            auto remotePort = getInt(message[2]);
            auto senderId = message[3].getString();
            auto remoteOriginalDevId = message[4].getString();
            
            if (senderId != instanceId_) {
                hardwareService_.handleRemoteDeviceConnection(devType, remoteIP, remoteOriginalDevId, remotePort);
            }
        } else if (message.size() == 4) {
            auto devType = (InstrumentType)getInt(message[0]);
            auto remoteIP = message[1].getString();
            auto senderId = message[2].getString();
            auto remoteOriginalDevId = message[3].getString();
            
            if (senderId != instanceId_) {
                hardwareService_.handleRemoteDeviceConnection(devType, remoteIP, remoteOriginalDevId, 0);
            }
        } else if (message.size() == 3) {
            auto devType = (InstrumentType)getInt(message[0]);
            auto remoteIP = message[1].getString();
            auto senderId = message[2].getString();
            
            if (senderId != instanceId_) {
                hardwareService_.handleRemoteDeviceConnection(devType, remoteIP, "", 0);
            }
        } else if (message.size() == 2) {
            auto devType = (InstrumentType)getInt(message[0]);
            auto remoteIP = message[1].getString();
            hardwareService_.handleRemoteDeviceConnection(devType, remoteIP, "", 0);
        } else if (message.size() == 1) {
            auto devType = (InstrumentType)getInt(message[0]);
            hardwareService_.handleRemoteDeviceConnection(devType, "127.0.0.1", "", 0);
        }
    } else if (pattern == "/EigenCore/key" && message.size() >= 7) {
        msg.type = osc::MessageType::Key;
        msg.course = (unsigned int)getInt(message[0]);
        msg.key = (unsigned int)getInt(message[1]);
        msg.active = getInt(message[2]);
        msg.pressure = getFloat(message[3]);
        msg.roll = getFloat(message[4]);
        msg.yaw = getFloat(message[5]);
        msg.device = (InstrumentType)getInt(message[6]);
        if (message.size() >= 8) std::strncpy(msg.devId, message[7].getString().toRawUTF8(), 63);
        
        if (std::strlen(msg.devId) > 0 && !hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
             // Auto-discover if we are in slave mode and receive data for unknown device
             if (hardwareService_.getAppRole() == AppRole::Slave) {
                 hardwareService_.handleRemoteDeviceConnection(msg.device, "Unknown", msg.devId, hardwareService_.getSlaveListenPort());
             }
        }

        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/EigenCore/breath" && message.size() >= 2) {
        msg.type = osc::MessageType::Breath;
        msg.value = getFloat(message[0]);
        msg.device = (InstrumentType)getInt(message[1]);
        if (message.size() >= 3) std::strncpy(msg.devId, message[2].getString().toRawUTF8(), 63);
        
        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/EigenCore/strip" && message.size() >= 4) {
        msg.type = osc::MessageType::Strip;
        msg.strip = (unsigned int)getInt(message[0]);
        msg.value = getFloat(message[1]);
        msg.active = getInt(message[2]);
        msg.device = (InstrumentType)getInt(message[3]);
        if (message.size() >= 5) std::strncpy(msg.devId, message[4].getString().toRawUTF8(), 63);
        
        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/EigenCore/pedal" && message.size() >= 3) {
        msg.type = osc::MessageType::Pedal;
        msg.pedal = (unsigned int)getInt(message[0]);
        msg.value = getFloat(message[1]);
        msg.device = (InstrumentType)getInt(message[2]);
        if (message.size() >= 4) std::strncpy(msg.devId, message[3].getString().toRawUTF8(), 63);
        
        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/ECMapper/led" && message.size() >= 4) {
        msg.type = osc::MessageType::LED;
        msg.course = (unsigned int)getInt(message[0]);
        msg.key = (unsigned int)getInt(message[1]);
        msg.value = (unsigned int)getInt(message[2]);
        msg.device = (InstrumentType)getInt(message[3]);
        if (message.size() >= 5) std::strncpy(msg.devId, message[4].getString().toRawUTF8(), 63);
        
        mapperToHardwareQueue_.add(msg);
    } else if (pattern == "/ECMapper/reset" && message.size() >= 1) {
        msg.type = osc::MessageType::Reset;
        msg.device = (InstrumentType)getInt(message[0]);
        if (message.size() >= 2) std::strncpy(msg.devId, message[1].getString().toRawUTF8(), 63);
        
        mapperToHardwareQueue_.add(msg);
    }
}

} // namespace ecm
