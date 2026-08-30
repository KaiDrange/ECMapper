#pragma once
#include <JuceHeader.h>
#include <eigenapi.h>
#include "OSCMessage.h"

namespace ecm {

class HardwareService : private juce::Thread, public EigenApi::Callback, public EigenApi::LifecycleCallback {
public:
    HardwareService(osc::MessageFifo& hardwareToMapperQueue, 
                    osc::MessageFifo& mapperToHardwareQueue);
    ~HardwareService() override;
    
    void startService();
    void stopService();
    bool isServiceRunning() const { return isThreadRunning(); }

    void turnOffAllLEDs();

    struct ConnectedDevice {
        std::string dev;
        std::string remoteOriginalDevId;
        ecm::InstrumentType type = ecm::InstrumentType::None;
        ecm::DeviceMode mode = ecm::DeviceMode::Local;
        bool isRemote = false;
        juce::String oscIP = "127.0.0.1";
        int oscPort = 12120;
        int assignedLEDColours[3][120] = { {0} };
        bool activeKeys[3][120] = { {false} };
    };

    std::vector<ConnectedDevice> getConnectedDevices();
    void setDeviceMode(const std::string& dev, ecm::DeviceMode mode);
    void setDeviceOSCSettings(const std::string& dev, const juce::String& ip, int port);
    bool isDeviceTypeInReceiveOSCMode(ecm::InstrumentType type);
    bool isDeviceInReceiveOSCMode(const std::string& dev);
    void handleRemoteDeviceConnection(ecm::InstrumentType type, const juce::String& remoteIP, const juce::String& remoteDevId, int port);

    AppRole getAppRole() const { return appRole_; }
    void setAppRole(AppRole role);
    
    juce::String getSlaveListenIP() const { return slaveListenIP_; }
    int getSlaveListenPort() const { return slaveListenPort_; }
    void setSlaveListenSettings(const juce::String& ip, int port);

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void deviceListChanged() = 0;
    };

    void addListener(Listener* listener) { listeners_.add(listener); }
    void removeListener(Listener* listener) { listeners_.remove(listener); }

    void setOSCBroadcastQueue(osc::MessageFifo* queue) { oscBroadcastQueue_ = queue; }

    // EigenApi::LifecycleCallback overrides
    void connected(const char* dev, EigenApi::DeviceType dt) override;
    void disconnected(const char* dev) override;

    // EigenApi::Callback overrides
    void key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, float p, float r, float y) override;
    void breath(const char* dev, unsigned long long t, float val) override;
    void strip(const char* dev, unsigned long long t, unsigned strip, float val, bool a) override;
    void pedal(const char* dev, unsigned long long t, unsigned pedal, float val) override;

private:
    void run() override;
    
    EigenApi::Eigenharp eigenApi_;
    
    osc::MessageFifo& hardwareToMapperQueue_;
    osc::MessageFifo& mapperToHardwareQueue_;
    osc::MessageFifo* oscBroadcastQueue_ = nullptr;
    
    std::vector<ConnectedDevice> connectedDevices_;
    juce::CriticalSection deviceListLock_;
    juce::ListenerList<Listener> listeners_;
    
    AppRole appRole_ = AppRole::Master;
    juce::String slaveListenIP_ = "127.0.0.1";
    int slaveListenPort_ = 12130;

    void processOutgoingMessages();
    ecm::InstrumentType getInstrumentTypeFromCols(int cols) const;
    ecm::InstrumentType getInstrumentTypeFromDev(const char* dev) const;
};

} // namespace ecm
