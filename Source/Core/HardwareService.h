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
    
    struct ConnectedDevice {
        std::string dev;
        ecm::InstrumentType type = ecm::InstrumentType::None;
        int assignedLEDColours[3][120] = { {0} };
        bool activeKeys[3][120] = { {false} };
    };
    
    std::vector<ConnectedDevice> connectedDevices_;
    juce::CriticalSection deviceListLock_;

    void processOutgoingMessages();
    ecm::InstrumentType getInstrumentTypeFromCols(int cols) const;
    ecm::InstrumentType getInstrumentTypeFromDev(const char* dev) const;
};

} // namespace ecm
