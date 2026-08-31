#pragma once
#include <JuceHeader.h>
#include "OSCMessage.h"
#include "Enums.h"

#if ECMAPPER_ENABLE_HARDWARE
#include <eigenapi.h>
#endif

namespace ecm {

class HardwareService : private juce::Thread
#if ECMAPPER_ENABLE_HARDWARE
    , public EigenApi::Callback, public EigenApi::LifecycleCallback
#endif
{
public:
    static constexpr bool supportsLocalHardware() noexcept {
#if ECMAPPER_ENABLE_HARDWARE
        return true;
#else
        return false;
#endif
    }

    HardwareService(osc::MessageFifo& hardwareToMapperQueue, 
                    osc::MessageFifo& mapperToHardwareQueue);
    ~HardwareService() override;
    
    void startService(juce::ValueTree* state = nullptr);
    void stopService();
    bool isServiceRunning() const { return isThreadRunning(); }

    void turnOffAllLEDs();
    void turnOffDeviceLEDs(const std::string& devId);

    std::vector<ConnectedDevice> getConnectedDevices();
    void setDeviceMode(const std::string& dev, ecm::DeviceMode mode);
    ecm::DeviceMode getDeviceMode(const std::string& devId) const;
    void addDeviceOSCTarget(const std::string& dev, const juce::String& ip, int port);
    void removeDeviceOSCTarget(const std::string& dev, int targetIndex);
    void updateDeviceOSCTarget(const std::string& dev, int targetIndex, const juce::String& ip, int port, bool receiveLEDs);
    bool isDeviceTypeInReceiveOSCMode(ecm::InstrumentType type);
    bool isDeviceInReceiveOSCMode(const std::string& dev);
    void handleRemoteDeviceConnection(ecm::InstrumentType type, const juce::String& remoteIP, const juce::String& remoteDevId, int port);
    void updateDeviceLastMessageTime(const std::string& devId);
    void checkStaleDevices();

    AppRole getAppRole() const { return appRole_; }
    void setAppRole(AppRole role);

    void syncLEDs(const std::string& devId);
    void handleLEDRequest(const std::string& devId);
    void requestRemoteLEDs(const std::string& devId);
    
    juce::String getClientListenIP() const { return clientListenIP_; }
    int getClientListenPort() const { return clientListenPort_; }
    void setClientListenSettings(const juce::String& ip, int port);

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void deviceListChanged() = 0;
        virtual void deviceNeedsLEDSync(const std::string& devId, InstrumentType type, bool isRequest) {}
    };

    void addListener(Listener* listener) { listeners_.add(listener); }
    void removeListener(Listener* listener) { listeners_.remove(listener); }

    void setOSCBroadcastQueue(osc::MessageFifo* queue) { oscBroadcastQueue_ = queue; }

    bool isDeviceAuthorizedForLEDs(const std::string& devId) const;

#if ECMAPPER_ENABLE_HARDWARE
    // EigenApi::LifecycleCallback overrides
    void connected(const char* dev, EigenApi::DeviceType dt) override;
    void disconnected(const char* dev) override;

    // EigenApi::Callback overrides
    void key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, float p, float r, float y) override;
    void breath(const char* dev, unsigned long long t, float val) override;
    void strip(const char* dev, unsigned long long t, unsigned strip, float val, bool a) override;
    void pedal(const char* dev, unsigned long long t, unsigned pedal, float val) override;
#endif

private:
    void run() override;

#if ECMAPPER_ENABLE_HARDWARE
    std::unique_ptr<EigenApi::Eigenharp> eigenApi_;
#endif
    
    juce::ValueTree* state_ = nullptr;
    
    osc::MessageFifo& hardwareToMapperQueue_;
    osc::MessageFifo& mapperToHardwareQueue_;
    osc::MessageFifo* oscBroadcastQueue_ = nullptr;
    
    std::vector<ConnectedDevice> connectedDevices_;
    juce::CriticalSection deviceListLock_;
    juce::ListenerList<Listener> listeners_;
    
    AppRole appRole_ = supportsLocalHardware() ? AppRole::Host : AppRole::Client;
    juce::String clientListenIP_ = "127.0.0.1";
    int clientListenPort_ = 12130;

    void processOutgoingMessages();
    ecm::InstrumentType getInstrumentTypeFromCols(int cols) const;
    ecm::InstrumentType getInstrumentTypeFromDev(const char* dev) const;
};

} // namespace ecm
