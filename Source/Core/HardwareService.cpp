#include "HardwareService.h"
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

void HardwareService::startService() {
    if (isThreadRunning()) return;
    
    eigenApi_.addCallback(this);
    eigenApi_.addLifecycleCallback(this);
    eigenApi_.setPollTime(100);
    
    if (!eigenApi_.start()) {
        std::cerr << "Unable to start EigenLite" << std::endl;
        return;
    }
    
    startThread();
}

void HardwareService::stopService() {
    if (!isThreadRunning()) return;
    
    signalThreadShouldExit();
    stopThread(2000);
    
    eigenApi_.stop();
    eigenApi_.removeCallback(this);
    eigenApi_.removeLifecycleCallback(this);
}

void HardwareService::turnOffAllLEDs() {
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        // Pico
        int course0Length = 0;
        int course1Start = 0;
        int course1Length = 0;
        
        switch (d.type) {
            case InstrumentType::Pico:
                course0Length = 18;
                course1Length = 4;
                break;
            case InstrumentType::Tau:
                course0Length = 72 + 12;
                course1Length = 8;
                course1Start = 5;
                break;
            case InstrumentType::Alpha:
                course0Length = 120;
                course1Length = 12;
                break;
            default: break;
        }

        for (int i = 0; i < course0Length; i++) {
            eigenApi_.setLED(d.dev.c_str(), 0, i, EigenApi::Eigenharp::LED_OFF);
            d.assignedLEDColours[0][i] = 0;
            d.activeKeys[0][i] = false;
        }
        for (int i = course1Start; i < course1Start + course1Length; i++) {
            eigenApi_.setLED(d.dev.c_str(), 1, i, EigenApi::Eigenharp::LED_OFF);
            d.assignedLEDColours[1][i] = 0;
            d.activeKeys[1][i] = false;
        }
    }
}

void HardwareService::run() {
    while (!threadShouldExit()) {
        try {
            eigenApi_.process();
        } catch (...) {
            std::cerr << "EigenAPI process() threw an exception." << std::endl;
        }
        
        processOutgoingMessages();
        
        wait(1);
    }
}

void HardwareService::processOutgoingMessages() {
    osc::Message msg;
    while (mapperToHardwareQueue_.read(msg)) {
        if (msg.type == osc::MessageType::LED) {
            const juce::ScopedLock sl(deviceListLock_);
            for (auto& d : connectedDevices_) {
                if (d.type == msg.device) {
                    d.assignedLEDColours[msg.course][msg.key] = (int)msg.value;
                    eigenApi_.setLED(d.dev.c_str(), msg.course, msg.key, (EigenApi::Eigenharp::LedColour)msg.value);
                }
            }
        } else if (msg.type == osc::MessageType::Reset) {
            turnOffAllLEDs();
        }
    }
}

// EigenApi::LifecycleCallback implementations
void HardwareService::connected(const char* dev, EigenApi::DeviceType dt) {
    InstrumentType devType = InstrumentType::None;
    switch (dt) {
        case EigenApi::PICO: devType = InstrumentType::Pico; break;
        case EigenApi::TAU: devType = InstrumentType::Tau; break;
        case EigenApi::ALPHA: devType = InstrumentType::Alpha; break;
    }
    
    const juce::ScopedLock sl(deviceListLock_);
    connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(),
        [devType](const ConnectedDevice& d) { return d.type == devType; }), connectedDevices_.end());
    
    ConnectedDevice newDev;
    newDev.dev = dev;
    newDev.type = devType;
    connectedDevices_.push_back(newDev);
    
    osc::Message msg;
    msg.type = osc::MessageType::Device;
    msg.device = devType;
    hardwareToMapperQueue_.add(msg);
    if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
}

void HardwareService::disconnected(const char* dev) {
    const juce::ScopedLock sl(deviceListLock_);
    connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(),
        [dev](const ConnectedDevice& d) { return d.dev == dev; }), connectedDevices_.end());
}

// EigenApi::Callback implementations
void HardwareService::key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, float p, float r, float y) {
    juce::ignoreUnused(t);
    if (course >= 3 || key >= 120) return;
    
    const juce::ScopedLock sl(deviceListLock_);
    for (auto& d : connectedDevices_) {
        if (d.dev == dev) {
            if (a && !d.activeKeys[course][key]) {
                d.activeKeys[course][key] = true;
                eigenApi_.setLED(dev, course, key, EigenApi::Eigenharp::LED_ORANGE);
            } else if (!a) {
                d.activeKeys[course][key] = false;
                eigenApi_.setLED(dev, course, key, (EigenApi::Eigenharp::LedColour)d.assignedLEDColours[course][key]);
            }
            
            osc::Message msg;
            msg.type = osc::MessageType::Key;
            msg.course = course;
            msg.key = key;
            msg.active = a;
            msg.pressure = p;
            msg.roll = r;
            msg.yaw = y;
            msg.device = d.type;
            
            if (a) {
                juce::Logger::writeToLog("HardwareService: Key Down - Course: " + juce::String(course) + ", Key: " + juce::String(key) + ", Pressure: " + juce::String(p));
            }

            hardwareToMapperQueue_.add(msg);
            if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
            break;
        }
    }
}

void HardwareService::breath(const char* dev, unsigned long long t, float val) {
    juce::ignoreUnused(t);
    osc::Message msg;
    msg.type = osc::MessageType::Breath;
    msg.value = val;
    msg.device = getInstrumentTypeFromDev(dev);
    hardwareToMapperQueue_.add(msg);
    if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
}

void HardwareService::strip(const char* dev, unsigned long long t, unsigned strip, float val, bool a) {
    juce::ignoreUnused(t);
    osc::Message msg;
    msg.type = osc::MessageType::Strip;
    msg.strip = strip;
    msg.value = val;
    msg.active = a;
    msg.device = getInstrumentTypeFromDev(dev);
    hardwareToMapperQueue_.add(msg);
    if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
}

void HardwareService::pedal(const char* dev, unsigned long long t, unsigned pedal, float val) {
    juce::ignoreUnused(t);
    osc::Message msg;
    msg.type = osc::MessageType::Pedal;
    msg.pedal = pedal;
    msg.value = val;
    msg.device = getInstrumentTypeFromDev(dev);
    hardwareToMapperQueue_.add(msg);
    if (oscBroadcastQueue_) oscBroadcastQueue_->add(msg);
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
