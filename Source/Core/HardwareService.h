#pragma once
#include <JuceHeader.h>
#include <eigenapi.h>
#include "OSCMessage.h"
#include "Enums.h"

namespace ecm {

class HardwareService : private juce::Thread, public EigenApi::Callback, public EigenApi::LifecycleCallback {
public:
    HardwareService(osc::MessageFifo& hardwareToMapperQueue, 
                    osc::MessageFifo& mapperToHardwareQueue);
    ~HardwareService() override;
    
    void startService(juce::ValueTree* state = nullptr);
    void stopService();
    bool isServiceRunning() const { return isThreadRunning(); }

    void turnOffAllLEDs();

    std::vector<ConnectedDevice> getConnectedDevices();
    void setDeviceMode(const std::string& dev, ecm::DeviceMode mode);
    void addDeviceOSCTarget(const std::string& dev, const juce::String& ip, int port);
    void removeDeviceOSCTarget(const std::string& dev, int targetIndex);
    void updateDeviceOSCTarget(const std::string& dev, int targetIndex, const juce::String& ip, int port);
    bool isDeviceTypeInReceiveOSCMode(ecm::InstrumentType type);
    bool isDeviceInReceiveOSCMode(const std::string& dev);
    void handleRemoteDeviceConnection(ecm::InstrumentType type, const juce::String& remoteIP, const juce::String& remoteDevId, int port);

    AppRole getAppRole() const { return appRole_; }
    void setAppRole(AppRole role);
    
    juce::String getClientListenIP() const { return clientListenIP_; }
    int getClientListenPort() const { return clientListenPort_; }
    void setClientListenSettings(const juce::String& ip, int port);

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
    
    std::unique_ptr<EigenApi::Eigenharp> eigenApi_;
    
    juce::ValueTree* state_ = nullptr;
    
    osc::MessageFifo& hardwareToMapperQueue_;
    osc::MessageFifo& mapperToHardwareQueue_;
    osc::MessageFifo* oscBroadcastQueue_ = nullptr;
    
    std::vector<ConnectedDevice> connectedDevices_;
    juce::CriticalSection deviceListLock_;
    juce::ListenerList<Listener> listeners_;
    
    AppRole appRole_ = AppRole::Host;
    juce::String clientListenIP_ = "127.0.0.1";
    int clientListenPort_ = 12130;

    void processOutgoingMessages();
    ecm::InstrumentType getInstrumentTypeFromCols(int cols) const;
    ecm::InstrumentType getInstrumentTypeFromDev(const char* dev) const;
};

} // namespace ecm
