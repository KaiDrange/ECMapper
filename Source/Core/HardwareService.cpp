#include "HardwareService.h"
#include "SettingsWrapper.h"
#include <iostream>

namespace ecm {

HardwareService::HardwareService(osc::MessageFifo& hardwareToMapperQueue, 
                                 osc::MessageFifo& mapperToHardwareQueue)
    : Thread("HardwareServiceThread"),
      hardwareToMapperQueue_(hardwareToMapperQueue),
      mapperToHardwareQueue_(mapperToHardwareQueue) {
}

HardwareService::~HardwareService() {
    stopService();
}

void HardwareService::startService(juce::ValueTree* state) {
    state_ = state;
    if (isThreadRunning()) return;
    
    if (state_) appRole_ = SettingsWrapper::getAppRole(*state_);

    if (!supportsLocalHardware() && appRole_ == AppRole::Host) {
        appRole_ = AppRole::Client;
        if (state_) SettingsWrapper::setAppRole(appRole_, *state_);
    }

#if ECMAPPER_ENABLE_HARDWARE
    // Re-create the Eigenharp instance to ensure a clean discovery state
    eigenApi_ = std::make_unique<EigenApi::Eigenharp>();

    eigenApi_->addCallback(this);
    eigenApi_->addLifecycleCallback(this);
    eigenApi_->setPollTime(100);

    if (appRole_ == AppRole::Host) {
        if (!eigenApi_->start()) {
            std::cerr << "Unable to start EigenLite" << std::endl;
        }
    }
#endif
    
    startThread();
}

void HardwareService::stopService() {
    if (!isThreadRunning()) return;
    
    signalThreadShouldExit();
    stopThread(2000);

#if ECMAPPER_ENABLE_HARDWARE
    if (eigenApi_) {
        eigenApi_->stop();
        eigenApi_->removeCallback(this);
        eigenApi_->removeLifecycleCallback(this);
        eigenApi_.reset();
    }
#endif
}

std::vector<ConnectedDevice> HardwareService::getConnectedDevices() {
    const juce::ScopedLock sl(deviceListLock_);
    return connectedDevices_;
}

void HardwareService::setDeviceMode(const std::string& dev, ecm::DeviceMode mode) {
    {
        const juce::ScopedLock sl(deviceListLock_);
        for (auto& d : connectedDevices_) {
            if (d.dev == dev) {
                ecm::DeviceMode oldMode = d.mode;
                d.mode = mode;
                
                // Add default target if switching to OSC mode and none exist
                if (d.mode != ecm::DeviceMode::Local && d.oscTargets.empty()) {
                    d.oscTargets.push_back({"127.0.0.1", 12130, true});
                }
                
                if (state_) SettingsWrapper::saveDeviceSettings(d, *state_);
                
                if (d.mode == ecm::DeviceMode::Local) {
                    syncLEDs(d.dev);
                } else if (d.mode == ecm::DeviceMode::TransmitOSC) {
                    if (oldMode != ecm::DeviceMode::TransmitOSC) {
                        turnOffDeviceLEDs(d.dev);
                        requestRemoteLEDs(d.dev);
                    }
                    
                    osc::Message msg;
                    msg.type = osc::MessageType::Device;
                    msg.device = d.type;
                    std::strncpy(msg.devId, dev.c_str(), 63);
                    if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
                }
                break;
            }
        }
    }
    listeners_.call(&Listener::deviceListChanged);
}

ecm::DeviceMode HardwareService::getDeviceMode(const std::string& devId) const {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == devId || d.remoteOriginalDevId == devId) return d.mode;
    }
    return ecm::DeviceMode::Local;
}

void HardwareService::addDeviceOSCTarget(const std::string& dev, const juce::String& ip, int port) {
    {
        const juce::ScopedLock sl(deviceListLock_);
        for (auto& d : connectedDevices_) {
            if (d.dev == dev) {
                if (d.oscTargets.size() < 3) {
                    int nextPort = port;
                    if (!d.oscTargets.empty()) {
                        nextPort = d.oscTargets.back().port + 2;
                    }
                    bool receiveLEDs = d.oscTargets.empty();
                    d.oscTargets.push_back({ip, nextPort, receiveLEDs});
                    if (state_) SettingsWrapper::saveDeviceSettings(d, *state_);
                }
                break;
            }
        }
    }
    listeners_.call(&Listener::deviceListChanged);
}

void HardwareService::removeDeviceOSCTarget(const std::string& dev, int targetIndex) {
    {
        const juce::ScopedLock sl(deviceListLock_);
        for (auto& d : connectedDevices_) {
            if (d.dev == dev) {
                if (targetIndex >= 0 && targetIndex < (int)d.oscTargets.size()) {
                    d.oscTargets.erase(d.oscTargets.begin() + targetIndex);
                    if (state_) SettingsWrapper::saveDeviceSettings(d, *state_);
                }
                break;
            }
        }
    }
    listeners_.call(&Listener::deviceListChanged);
}

void HardwareService::updateDeviceOSCTarget(const std::string& dev, int targetIndex, const juce::String& ip, int port, bool receiveLEDs) {
    {
        const juce::ScopedLock sl(deviceListLock_);
        for (auto& d : connectedDevices_) {
            if (d.dev == dev) {
                if (targetIndex >= 0 && (size_t)targetIndex < d.oscTargets.size()) {
                    bool wasOn = d.oscTargets[(size_t)targetIndex].receiveLEDs;
                    d.oscTargets[(size_t)targetIndex] = {ip, port, receiveLEDs};
                    if (state_) SettingsWrapper::saveDeviceSettings(d, *state_);
                    
                    if (receiveLEDs && !wasOn) {
                        syncLEDs(d.dev);
                    }
                }
                break;
            }
        }
    }
    listeners_.call(&Listener::deviceListChanged);
}

bool HardwareService::isDeviceTypeInReceiveOSCMode(ecm::InstrumentType type) {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.type == type && d.mode == ecm::DeviceMode::ReceiveOSC) return true;
    }
    return false;
}

bool HardwareService::isDeviceInReceiveOSCMode(const std::string& dev) {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == dev || d.remoteOriginalDevId == dev) return d.mode == ecm::DeviceMode::ReceiveOSC;
    }
    return false;
}

void HardwareService::turnOffAllLEDs() {
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (!d.isRemote) {
            turnOffDeviceLEDs(d.dev);
        }
    }
}

void HardwareService::turnOffDeviceLEDs(const std::string& devId) {
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (d.dev == devId && !d.isRemote) {
            for (int course = 0; course < 3; course++) {
                for (int key = 0; key < 120; key++) {
#if ECMAPPER_ENABLE_HARDWARE
                    if (eigenApi_) eigenApi_->setLED(d.dev.c_str(), (unsigned)course, (unsigned)key, EigenApi::Eigenharp::LED_OFF);
#endif
                    d.assignedLEDColours[course][key] = 0;
                    d.activeKeys[course][key] = false;
                }
            }
            break;
        }
    }
}

void HardwareService::run() {
    while (!threadShouldExit()) {
#if ECMAPPER_ENABLE_HARDWARE
        if (appRole_ == AppRole::Host && eigenApi_) {
            try {
                eigenApi_->process();
            } catch (...) {
                std::cerr << "EigenAPI process() threw an exception." << std::endl;
            }
        }
#endif
        
        processOutgoingMessages();
        
        if (appRole_ == AppRole::Client) {
            static uint32_t lastStaleCheck = 0;
            uint32_t now = juce::Time::getMillisecondCounter();
            if (now - lastStaleCheck > 1000) {
                checkStaleDevices();
                lastStaleCheck = now;
            }
        }
        
        wait(1);
    }
}

bool HardwareService::isDeviceAuthorizedForLEDs(const std::string& devId) const {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == devId) {
            if (d.isRemote) {
                // For remote devices (Client mode), we check the first target (the host we are receiving from)
                return !d.oscTargets.empty() && d.oscTargets[0].receiveLEDs;
            } else {
                // For local devices (Host mode), we check if ANY target is authorized to send back LEDs 
                // OR if local control is enabled (though MidiService handles local separately)
                // Actually, for a Host, we just check if it's in Transmit mode and has an authorized target
                for (const auto& t : d.oscTargets) {
                    if (t.receiveLEDs) return true;
                }
                return d.mode == ecm::DeviceMode::Local;
            }
        }
    }
    return false;
}

void HardwareService::processOutgoingMessages() {
    osc::Message msg;
    while (mapperToHardwareQueue_.read(msg)) {
        if (msg.type == osc::MessageType::LED) {
            const juce::ScopedLock sl(deviceListLock_);
            for (auto& d : connectedDevices_) {
                bool match = false;
                if (std::strlen(msg.devId) > 0) {
                    if (d.dev == msg.devId) match = true;
                } else {
                    if (d.type == msg.device) match = true;
                }

                if (match) {
                    if (d.isRemote) {
                        if (oscBroadcastQueue_) {
                            // If devId was empty, we should fill it for the specific remote device 
                            // so the host knows which one it is (in case of multiple hosts)
                            osc::Message remoteMsg = msg;
                            std::strncpy(remoteMsg.devId, d.dev.c_str(), 63);
                            oscBroadcastQueue_->add(remoteMsg);
                        }
                    } else {
                        if (d.mode == ecm::DeviceMode::Local || std::strlen(msg.devId) > 0) {
                            d.assignedLEDColours[msg.course][msg.key] = (int)msg.value;
#if ECMAPPER_ENABLE_HARDWARE
                            if (eigenApi_) eigenApi_->setLED(d.dev.c_str(), msg.course, msg.key, (EigenApi::Eigenharp::LedColour)msg.value);
#endif
                        }
                    }
                    if (std::strlen(msg.devId) > 0) break;
                }
            }
        } else if (msg.type == osc::MessageType::Reset) {
            const juce::ScopedLock sl(deviceListLock_);
            bool remoteResetQueued = false;
            for (auto& d : connectedDevices_) {
                if (msg.device == InstrumentType::None || d.type == msg.device) {
                    bool devIdMatch = (std::strlen(msg.devId) == 0 || d.dev == msg.devId);
                    if (!devIdMatch) continue;

                    if (d.isRemote) {
                        if (!remoteResetQueued && oscBroadcastQueue_) {
                            oscBroadcastQueue_->add(msg);
                            remoteResetQueued = true;
                        }
                    } else {
                        if (d.mode == ecm::DeviceMode::Local || std::strlen(msg.devId) > 0) {
                            turnOffDeviceLEDs(d.dev);
                        }
                    }
                }
            }
        }
    }
}

// EigenApi::LifecycleCallback implementations
#if ECMAPPER_ENABLE_HARDWARE
void HardwareService::connected(const char* dev, EigenApi::DeviceType dt) {
    InstrumentType devType = InstrumentType::None;
    switch (dt) {
        case EigenApi::PICO: devType = InstrumentType::Pico; break;
        case EigenApi::TAU: devType = InstrumentType::Tau; break;
        case EigenApi::ALPHA: devType = InstrumentType::Alpha; break;
    }
    
    const juce::ScopedLock sl(deviceListLock_);
    connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(),
        [dev](const ConnectedDevice& d) { return d.dev == dev; }), connectedDevices_.end());
    
    ConnectedDevice newDev;
    newDev.dev = dev;
    newDev.type = devType;
    if (state_) SettingsWrapper::loadDeviceSettings(newDev, *state_);
    
    // Sanitize mode based on role
    if (appRole_ == AppRole::Host && newDev.mode == ecm::DeviceMode::ReceiveOSC) {
        newDev.mode = ecm::DeviceMode::Local;
    } else if (appRole_ == AppRole::Client) {
        newDev.mode = ecm::DeviceMode::ReceiveOSC;
    }

    connectedDevices_.push_back(newDev);
    
    listeners_.call(&Listener::deviceListChanged);
    syncLEDs(newDev.dev);

    osc::Message msg;
    msg.type = osc::MessageType::Device;
    msg.device = devType;
    std::strncpy(msg.devId, dev, 63);
    hardwareToMapperQueue_.add(msg);
    if (oscBroadcastQueue_ && newDev.mode == ecm::DeviceMode::TransmitOSC) 
        oscBroadcastQueue_->add(msg);
}

void HardwareService::disconnected(const char* dev) {
    const juce::ScopedLock sl(deviceListLock_);
    connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(),
        [dev](const ConnectedDevice& d) { return d.dev == dev; }), connectedDevices_.end());
    listeners_.call(&Listener::deviceListChanged);
}

// EigenApi::Callback implementations
void HardwareService::key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, float p, float r, float y) {
    if (course >= 3 || key >= 120) return;
    
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (d.dev == dev) {
            if (d.mode == ecm::DeviceMode::ReceiveOSC) return;

            if (d.mode == ecm::DeviceMode::Local) {
                if (a && !d.activeKeys[course][key]) {
                    d.activeKeys[course][key] = true;
                    if (eigenApi_) eigenApi_->setLED(dev, course, key, EigenApi::Eigenharp::LED_ORANGE);
                } else if (!a) {
                    d.activeKeys[course][key] = false;
                    if (eigenApi_) eigenApi_->setLED(dev, course, key, (EigenApi::Eigenharp::LedColour)d.assignedLEDColours[course][key]);
                }
            }
            
            osc::Message msg;
            msg.type = osc::MessageType::Key;
            msg.course = course;
            msg.key = key;
            msg.active = a;
            msg.pressure = p;
            msg.roll = r;
            msg.yaw = y;
            msg.timestamp = t;
            msg.device = d.type;
            std::strncpy(msg.devId, dev, 63);
            
            if (d.mode == ecm::DeviceMode::Local) {
                hardwareToMapperQueue_.add(msg);
            } else if (d.mode == ecm::DeviceMode::TransmitOSC) {
                if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
            }
            
            if (a) {
                juce::Logger::writeToLog("HardwareService: Key Down - Course: " + juce::String(course) + ", Key: " + juce::String(key) + ", Pressure: " + juce::String(p));
            }
            break;
        }
    }
}

void HardwareService::breath(const char* dev, unsigned long long t, float val) {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == dev) {
            if (d.mode == ecm::DeviceMode::ReceiveOSC) return;
            
            osc::Message msg;
            msg.type = osc::MessageType::Breath;
            msg.value = val;
            msg.timestamp = t;
            msg.device = d.type;
            std::strncpy(msg.devId, dev, 63);
            
            if (d.mode == ecm::DeviceMode::Local) {
                hardwareToMapperQueue_.add(msg);
            } else if (d.mode == ecm::DeviceMode::TransmitOSC) {
                if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
            }
            break;
        }
    }
}

void HardwareService::strip(const char* dev, unsigned long long t, unsigned strip, float val, bool a) {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == dev) {
            if (d.mode == ecm::DeviceMode::ReceiveOSC) return;

            osc::Message msg;
            msg.type = osc::MessageType::Strip;
            msg.strip = strip;
            msg.value = val;
            msg.active = a;
            msg.timestamp = t;
            msg.device = d.type;
            std::strncpy(msg.devId, dev, 63);
            
            if (d.mode == ecm::DeviceMode::Local) {
                hardwareToMapperQueue_.add(msg);
            } else if (d.mode == ecm::DeviceMode::TransmitOSC) {
                if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
            }
            break;
        }
    }
}

void HardwareService::pedal(const char* dev, unsigned long long t, unsigned pedal, float val) {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == dev) {
            if (d.mode == ecm::DeviceMode::ReceiveOSC) return;

            osc::Message msg;
            msg.type = osc::MessageType::Pedal;
            msg.pedal = pedal;
            msg.value = val;
            msg.timestamp = t;
            msg.device = d.type;
            std::strncpy(msg.devId, dev, 63);
            
            if (d.mode == ecm::DeviceMode::Local) {
                hardwareToMapperQueue_.add(msg);
            } else if (d.mode == ecm::DeviceMode::TransmitOSC) {
                if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
            }
            break;
        }
    }
}
#endif

void HardwareService::handleRemoteDeviceConnection(ecm::InstrumentType type, const juce::String& remoteIP, const juce::String& remoteDevId, int port) {
    std::string stdRemoteDevId = remoteDevId.toStdString();
    
    const juce::ScopedLock sl(deviceListLock_);
    
    // Check if we already have this device
    for (auto& d : connectedDevices_) {
        if (d.isRemote && d.remoteOriginalDevId == stdRemoteDevId) {
            auto currentTime = juce::Time::getMillisecondCounter();
            bool wasTimedOut = (currentTime - d.lastMessageTime > 2000);
            d.lastMessageTime = currentTime;
            
            bool changed = false;
            // If we have a real IP now and previously it was "Unknown", update it
            if (remoteIP != "Unknown" && (d.oscTargets.empty() || d.oscTargets[0].ip == "Unknown")) {
                if (d.oscTargets.empty()) {
                    d.oscTargets.push_back({remoteIP, port, true});
                } else {
                    d.oscTargets[0].ip = remoteIP;
                    d.oscTargets[0].port = port;
                }
                d.dev = "Remote-" + stdRemoteDevId + "@" + remoteIP.toStdString();
                changed = true;
            }
            if (port > 0 && (d.oscTargets.empty() || d.oscTargets[0].port != port)) {
                if (d.oscTargets.empty()) {
                    d.oscTargets.push_back({remoteIP, port, true});
                } else {
                    d.oscTargets[0].port = port;
                }
                changed = true;
            }
            
            if (wasTimedOut || changed) {
                listeners_.call(&Listener::deviceNeedsLEDSync, d.dev, d.type, false);
            }

            if (changed) {
                listeners_.call(&Listener::deviceListChanged);
            }
            return;
        }
    }
    
    std::string fullRemoteDevId = "Remote-" + stdRemoteDevId + "@" + remoteIP.toStdString();
    
    ConnectedDevice newDev;
    newDev.dev = fullRemoteDevId;
    newDev.remoteOriginalDevId = stdRemoteDevId;
    newDev.type = type;
    newDev.isRemote = true;
    if (state_) SettingsWrapper::loadDeviceSettings(newDev, *state_);
    if (newDev.oscTargets.empty()) {
        newDev.oscTargets.push_back({remoteIP, port > 0 ? port : 12120, true});
    }
    newDev.mode = ecm::DeviceMode::ReceiveOSC; // Default for remote devices
    newDev.lastMessageTime = juce::Time::getMillisecondCounter();
    connectedDevices_.push_back(newDev);
    
    listeners_.call(&Listener::deviceListChanged);
    listeners_.call(&Listener::deviceNeedsLEDSync, newDev.dev, newDev.type, false);
}

void HardwareService::syncLEDs(const std::string& devId) {
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (d.dev == devId) {
            listeners_.call(&Listener::deviceNeedsLEDSync, d.dev, d.type, false);
            break;
        }
    }
}

void HardwareService::handleLEDRequest(const std::string& devId) {
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (d.dev == devId || d.remoteOriginalDevId == devId) {
            listeners_.call(&Listener::deviceNeedsLEDSync, d.dev, d.type, true);
            break;
        }
    }
}

void HardwareService::requestRemoteLEDs(const std::string& devId) {
    if (oscBroadcastQueue_) {
        osc::Message msg;
        msg.type = osc::MessageType::RequestLEDs;
        std::strncpy(msg.devId, devId.c_str(), 63);
        oscBroadcastQueue_->add(msg);
    }
}

void HardwareService::updateDeviceLastMessageTime(const std::string& devId) {
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (d.dev == devId || d.remoteOriginalDevId == devId) {
            d.lastMessageTime = juce::Time::getMillisecondCounter();
            return;
        }
    }
}

void HardwareService::checkStaleDevices() {
    const juce::ScopedLock sl(deviceListLock_);
    auto currentTime = juce::Time::getMillisecondCounter();
    
    bool changed = false;
    for (int i = (int)connectedDevices_.size(); --i >= 0;) {
        auto& d = connectedDevices_[i];
        if (d.isRemote) {
            // If no message for 60 seconds, remove
            if (currentTime - d.lastMessageTime > 60000) {
                connectedDevices_.erase(connectedDevices_.begin() + i);
                changed = true;
            }
        }
    }
    
    if (changed) {
        // We can't call listeners directly from the HardwareService thread easily if it updates UI
        // But the listeners should handle thread safety (e.g. using MessageManager::callAsync)
        listeners_.call(&Listener::deviceListChanged);
    }
}

void HardwareService::setAppRole(AppRole role) {
    if (!supportsLocalHardware()) {
        role = AppRole::Client;
    }

    if (state_) SettingsWrapper::setAppRole(role, *state_);

    if (appRole_ == role) return;
    
    if (appRole_ == AppRole::Host) {
        turnOffAllLEDs();
    }
    
    stopService();
    
    appRole_ = role;
    
    {
        const juce::ScopedLock sl(deviceListLock_);
        if (appRole_ == AppRole::Client) {
            // Remove local devices
            connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(), 
                [](const ConnectedDevice& d) { return !d.isRemote; }), connectedDevices_.end());
        } else {
            // Host mode
            // Remove remote devices
            connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(), 
                [](const ConnectedDevice& d) { return d.isRemote; }), connectedDevices_.end());
            
            // Sanitize local devices: Host cannot be in Receive mode
            for (auto& d : connectedDevices_) {
                if (d.mode == ecm::DeviceMode::ReceiveOSC) d.mode = ecm::DeviceMode::Local;
            }
        }
    }
    
    startService(state_);
    
    if (appRole_ == AppRole::Host) {
        const juce::ScopedLock sl(deviceListLock_);
        for (auto& d : connectedDevices_) {
            if (d.mode == ecm::DeviceMode::Local) {
                syncLEDs(d.dev);
            } else if (d.mode == ecm::DeviceMode::TransmitOSC) {
                requestRemoteLEDs(d.dev);
            }
        }
    }
    
    listeners_.call(&Listener::deviceListChanged);
}

void HardwareService::setClientListenSettings(const juce::String& ip, int port) {
    if (clientListenIP_ == ip && clientListenPort_ == port) return;
    clientListenIP_ = ip;
    clientListenPort_ = port;
    listeners_.call(&Listener::deviceListChanged);
}

InstrumentType HardwareService::getInstrumentTypeFromCols(int cols) const {
    switch (cols) {
        case 2: return InstrumentType::Pico;
        case 4: return InstrumentType::Tau;
        case 5: return InstrumentType::Alpha;
        default: return InstrumentType::None;
    }
}

InstrumentType HardwareService::getInstrumentTypeFromDev(const char* dev) const {
    const juce::ScopedLock sl(deviceListLock_);
    for (const auto& d : connectedDevices_) {
        if (d.dev == dev) return d.type;
    }
    return InstrumentType::None;
}

} // namespace ecm
