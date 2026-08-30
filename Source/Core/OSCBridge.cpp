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
    updateClientReceiver();
}

void OSCBridge::updateClientReceiver() {
    if (hardwareService_.getAppRole() == AppRole::Client) {
        int port = hardwareService_.getClientListenPort();
        if (globalClientReceiver_ == nullptr || !globalClientReceiver_->connect(port)) {
            globalClientReceiver_ = std::make_unique<juce::OSCReceiver>();
            if (globalClientReceiver_->connect(port)) {
                globalClientReceiver_->addListener(this);
                logger_.log("Global Client Receiver listening on port " + juce::String(port));
            } else {
                logger_.log("Global Client Receiver FAILED to listen on port " + juce::String(port));
            }
        }
    } else {
        if (globalClientReceiver_ != nullptr) {
            globalClientReceiver_->removeListener(this);
            globalClientReceiver_->disconnect();
            globalClientReceiver_ = nullptr;
        }
    }
}

void OSCBridge::updateConnections() {
    const juce::ScopedLock sl(connectionsLock_);
    connections_.clear();
    
    if (!hostEnabled_) {
        stopTimer();
        return;
    }
    
    auto devices = hardwareService_.getConnectedDevices();
    for (const auto& d : devices) {
        if (d.mode == ecm::DeviceMode::Local) continue;
        
        for (const auto& target : d.oscTargets) {
            auto conn = std::make_unique<Connection>();
            conn->dev = d.dev;
            conn->originalDevId = d.isRemote ? d.remoteOriginalDevId : d.dev;
            conn->type = d.type;
            conn->mode = d.mode;
            conn->ip = target.ip;
            conn->receiveLEDs = target.receiveLEDs;
            
            if (d.mode == ecm::DeviceMode::TransmitOSC) {
                conn->sendPort = target.port;
                conn->receivePort = target.port + 1;
            } else {
                conn->sendPort = target.port + 1;
                conn->receivePort = target.port;
            }
            
            conn->sender = std::make_unique<juce::OSCSender>();
            if (conn->sender->connect(conn->ip, conn->sendPort)) {
                logger_.log("OSC Sender connected to " + conn->ip + ":" + juce::String(conn->sendPort) + " for " + d.dev);
            } else {
                logger_.log("OSC Sender FAILED to connect to " + conn->ip + ":" + juce::String(conn->sendPort) + " for " + d.dev);
            }
            
            bool needsReceiver = true;
            if (globalClientReceiver_ != nullptr && conn->receivePort == hardwareService_.getClientListenPort()) {
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
                logger_.log("OSC Receiver using global client receiver for " + d.dev);
            }
            
            connections_.push_back(std::move(conn));
        }
    }
    
    if (hostEnabled_) {
        if (!isTimerRunning()) startTimer(100);
    } else {
        stopTimer();
    }
}

bool OSCBridge::isPortOccupied(int port) {
    juce::DatagramSocket socket;
    if (socket.bindToPort(port)) {
        return false;
    }
    return true;
}

void OSCBridge::setSenderEnabled(bool enabled) {
    if (hostEnabled_ == enabled) return;
    hostEnabled_ = enabled;
    updateConnections();
}

void OSCBridge::setReceiverEnabled(bool enabled) {
    if (enabled) {
        if (discoveryReceiver_.connect(12121)) {
            discoveryReceiver_.addListener(this);
            logger_.log("Discovery OSC Receiver listening on port 12121");
            discoveryPortBusy_ = false;
        } else {
            logger_.log("Discovery OSC Receiver FAILED to listen on port 12121");
            discoveryPortBusy_ = true;
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
            if (!d.isRemote && d.mode == ecm::DeviceMode::TransmitOSC) {
                juce::String localIP = juce::IPAddress::getLocalAddress().toString();
                int port = d.oscTargets.empty() ? 12130 : d.oscTargets[0].port;
                
                // Send to global discovery port
                discoverySender_.send("/EigenCore/device", (int)d.type, localIP, port, instanceId_, juce::String(d.dev));

                // Also send to all active Transmit connections
                for (auto& conn : connections_) {
                    if (conn->mode == ecm::DeviceMode::TransmitOSC) {
                        conn->sender->send("/EigenCore/device", (int)d.type, localIP, port, instanceId_, juce::String(d.dev));
                    }
                }
            }
        }
    }
    
    sendOutgoingMessages();
}

void OSCBridge::sendPing(Connection* conn) {
    if (conn && conn->sender) {
        conn->sender->send("/ECMapper/ping", juce::String(conn->dev), instanceId_);
    }
}

void OSCBridge::sendOutgoingMessages() {
    osc::Message msg;
    while (outgoingOSCQueue_.read(msg)) {
        if (msg.type == osc::MessageType::Device) {
            int port = 12130;
            auto devices = hardwareService_.getConnectedDevices();
            bool shouldSend = false;
            for (const auto& d : devices) {
                if (d.dev == msg.devId) {
                    if (d.mode == ecm::DeviceMode::TransmitOSC) {
                        if (!d.oscTargets.empty()) port = d.oscTargets[0].port;
                        shouldSend = true;
                    }
                    break;
                }
            }
            if (shouldSend) {
                discoverySender_.send("/EigenCore/device", (int)msg.device, juce::IPAddress::getLocalAddress().toString(), port, instanceId_, juce::String(msg.devId)); 
            }
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
                if (isPerformanceMsg && conn->mode == ecm::DeviceMode::TransmitOSC) {
                    shouldSend = true;
                } else if (!isPerformanceMsg && conn->mode == ecm::DeviceMode::ReceiveOSC) {
                    if (msg.type == osc::MessageType::LED || msg.type == osc::MessageType::Reset) {
                        if (conn->receiveLEDs) shouldSend = true;
                    } else {
                        shouldSend = true;
                    }
                }
            }

            if (shouldSend) {
                if (std::strlen(msg.devId) > 0 && conn->dev != msg.devId) continue;

                switch (msg.type) {
                    case osc::MessageType::Key:
                        conn->sender->send("/EigenCore/key", (int)msg.course, (int)msg.key, (int)msg.active, msg.pressure, msg.roll, msg.yaw, (int)msg.device, (int32_t)(msg.timestamp >> 32), (int32_t)(msg.timestamp & 0xFFFFFFFF), juce::String(conn->originalDevId), instanceId_);
                        break;
                    case osc::MessageType::Breath:
                        conn->sender->send("/EigenCore/breath", msg.value, (int)msg.device, (int32_t)(msg.timestamp >> 32), (int32_t)(msg.timestamp & 0xFFFFFFFF), juce::String(conn->originalDevId), instanceId_);
                        break;
                    case osc::MessageType::Strip:
                        conn->sender->send("/EigenCore/strip", (int)msg.strip, msg.value, (int)msg.active, (int)msg.device, (int32_t)(msg.timestamp >> 32), (int32_t)(msg.timestamp & 0xFFFFFFFF), juce::String(conn->originalDevId), instanceId_);
                        break;
                    case osc::MessageType::Pedal:
                        conn->sender->send("/EigenCore/pedal", (int)msg.pedal, msg.value, (int)msg.device, (int32_t)(msg.timestamp >> 32), (int32_t)(msg.timestamp & 0xFFFFFFFF), juce::String(conn->originalDevId), instanceId_);
                        break;
                    case osc::MessageType::Device:
                        conn->sender->send("/EigenCore/device", (int)msg.device, juce::IPAddress::getLocalAddress().toString(), conn->sendPort, instanceId_, juce::String(conn->originalDevId));
                        break;
                    case osc::MessageType::LED:
                        conn->sender->send("/ECMapper/led", (int)msg.course, (int)msg.key, (int)msg.value, (int)msg.device, juce::String(conn->originalDevId), instanceId_);
                        break;
                    case osc::MessageType::Reset:
                        conn->sender->send("/ECMapper/reset", (int)msg.device, juce::String(conn->originalDevId), instanceId_);
                        break;
                    case osc::MessageType::RequestLEDs:
                        conn->sender->send("/ECMapper/requestLEDs", juce::String(conn->originalDevId), instanceId_);
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
    auto getTimestamp = [](const juce::OSCArgument& high, const juce::OSCArgument& low) {
        uint64_t t = (uint64_t)(uint32_t)high.getInt32() << 32;
        t |= (uint64_t)(uint32_t)low.getInt32();
        return t;
    };

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
            // No instanceId, but we can't filter. Assume remote for now or ignore?
            // Let's at least check if it's not us if we had an IP, but discovery is 127.0.0.1 often.
            hardwareService_.handleRemoteDeviceConnection(devType, remoteIP, "", 0);
        } else if (message.size() == 1) {
            auto devType = (InstrumentType)getInt(message[0]);
            hardwareService_.handleRemoteDeviceConnection(devType, "127.0.0.1", "", 0);
        }
    } else if (pattern == "/EigenCore/key" && message.size() >= 9) {
        msg.type = osc::MessageType::Key;
        msg.course = (unsigned int)getInt(message[0]);
        msg.key = (unsigned int)getInt(message[1]);
        msg.active = getInt(message[2]);
        msg.pressure = getFloat(message[3]);
        msg.roll = getFloat(message[4]);
        msg.yaw = getFloat(message[5]);
        msg.device = (InstrumentType)getInt(message[6]);
        msg.timestamp = getTimestamp(message[7], message[8]);
        
        juce::String senderId;
        if (message.size() >= 10) {
            // Check if 10th arg is devId or instanceId
            if (message.size() >= 11) {
                std::strncpy(msg.devId, message[9].getString().toRawUTF8(), 63);
                senderId = message[10].getString();
            } else {
                auto str = message[9].getString();
                if (str == instanceId_) senderId = str;
                else std::strncpy(msg.devId, str.toRawUTF8(), 63);
            }
        }
        
        if (senderId == instanceId_) return;

        hardwareService_.updateDeviceLastMessageTime(msg.devId);

        if (std::strlen(msg.devId) > 0 && !hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
             // Auto-discover if we are in client mode and receive data for unknown device
             if (hardwareService_.getAppRole() == AppRole::Client) {
                 hardwareService_.handleRemoteDeviceConnection(msg.device, "Unknown", msg.devId, hardwareService_.getClientListenPort());
             }
        }

        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            msg.isRemote = 1;
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/EigenCore/breath" && message.size() >= 4) {
        msg.type = osc::MessageType::Breath;
        msg.value = getFloat(message[0]);
        msg.device = (InstrumentType)getInt(message[1]);
        msg.timestamp = getTimestamp(message[2], message[3]);
        
        juce::String senderId;
        if (message.size() >= 5) {
            if (message.size() >= 6) {
                std::strncpy(msg.devId, message[4].getString().toRawUTF8(), 63);
                senderId = message[5].getString();
            } else {
                auto str = message[4].getString();
                if (str == instanceId_) senderId = str;
                else std::strncpy(msg.devId, str.toRawUTF8(), 63);
            }
        }
        
        if (senderId == instanceId_) return;
        
        hardwareService_.updateDeviceLastMessageTime(msg.devId);
        
        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            msg.isRemote = 1;
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/EigenCore/strip" && message.size() >= 6) {
        msg.type = osc::MessageType::Strip;
        msg.strip = (unsigned int)getInt(message[0]);
        msg.value = getFloat(message[1]);
        msg.active = getInt(message[2]);
        msg.device = (InstrumentType)getInt(message[3]);
        msg.timestamp = getTimestamp(message[4], message[5]);
        
        juce::String senderId;
        if (message.size() >= 7) {
            if (message.size() >= 8) {
                std::strncpy(msg.devId, message[6].getString().toRawUTF8(), 63);
                senderId = message[7].getString();
            } else {
                auto str = message[6].getString();
                if (str == instanceId_) senderId = str;
                else std::strncpy(msg.devId, str.toRawUTF8(), 63);
            }
        }
        
        if (senderId == instanceId_) return;
        
        hardwareService_.updateDeviceLastMessageTime(msg.devId);
        
        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            msg.isRemote = 1;
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/EigenCore/pedal" && message.size() >= 5) {
        msg.type = osc::MessageType::Pedal;
        msg.pedal = (unsigned int)getInt(message[0]);
        msg.value = getFloat(message[1]);
        msg.device = (InstrumentType)getInt(message[2]);
        msg.timestamp = getTimestamp(message[3], message[4]);
        
        juce::String senderId;
        if (message.size() >= 6) {
            if (message.size() >= 7) {
                std::strncpy(msg.devId, message[5].getString().toRawUTF8(), 63);
                senderId = message[6].getString();
            } else {
                auto str = message[5].getString();
                if (str == instanceId_) senderId = str;
                else std::strncpy(msg.devId, str.toRawUTF8(), 63);
            }
        }
        
        if (senderId == instanceId_) return;
        
        hardwareService_.updateDeviceLastMessageTime(msg.devId);
        
        if (hardwareService_.isDeviceInReceiveOSCMode(msg.devId)) {
            msg.isRemote = 1;
            hardwareToMapperQueue_.add(msg);
        }
    } else if (pattern == "/ECMapper/led" && message.size() >= 4) {
        msg.type = osc::MessageType::LED;
        msg.course = (unsigned int)getInt(message[0]);
        msg.key = (unsigned int)getInt(message[1]);
        msg.value = (unsigned int)getInt(message[2]);
        msg.device = (InstrumentType)getInt(message[3]);
        
        juce::String senderId;
        if (message.size() >= 5) {
            if (message.size() >= 6) {
                std::strncpy(msg.devId, message[4].getString().toRawUTF8(), 63);
                senderId = message[5].getString();
            } else {
                auto str = message[4].getString();
                if (str == instanceId_) senderId = str;
                else std::strncpy(msg.devId, str.toRawUTF8(), 63);
            }
        }
        
        if (senderId == instanceId_) return;
        
        hardwareService_.updateDeviceLastMessageTime(msg.devId);
        
        mapperToHardwareQueue_.add(msg);
    } else if (pattern == "/ECMapper/requestLEDs" && message.size() >= 1) {
        juce::String devId = message[0].getString();
        juce::String senderId;
        if (message.size() >= 2) senderId = message[1].getString();
        
        if (senderId != instanceId_) {
            hardwareService_.handleLEDRequest(devId.toStdString());
        }
    } else if (pattern == "/ECMapper/ping" && message.size() >= 2) {
        auto devId = message[0].getString();
        auto senderId = message[1].getString();
        if (senderId != instanceId_) {
            hardwareService_.updateDeviceLastMessageTime(devId.toStdString());
        }
    } else if (pattern == "/ECMapper/reset" && message.size() >= 1) {
        msg.type = osc::MessageType::Reset;
        msg.device = (InstrumentType)getInt(message[0]);
        
        juce::String senderId;
        if (message.size() >= 2) {
            if (message.size() >= 3) {
                std::strncpy(msg.devId, message[1].getString().toRawUTF8(), 63);
                senderId = message[2].getString();
            } else {
                auto str = message[1].getString();
                if (str == instanceId_) senderId = str;
                else std::strncpy(msg.devId, str.toRawUTF8(), 63);
            }
        }
        
        if (senderId == instanceId_) return;
        
        mapperToHardwareQueue_.add(msg);
    }
}

} // namespace ecm
