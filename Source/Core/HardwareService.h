#pragma once
#include <JuceHeader.h>
#include <eigenapi.h>
#include "FirmwareReader.h"
#include "OSCMessage.h"

namespace ecm {

class HardwareService : private juce::Thread, public EigenApi::Callback {
public:
    HardwareService(osc::MessageFifo& hardwareToMapperQueue, 
                    osc::MessageFifo& mapperToHardwareQueue);
    ~HardwareService() override;
    
    void startService();
    void stopService();
    bool isServiceRunning() const { return isThreadRunning(); }

    void turnOffAllLEDs();

    void setOSCBroadcastQueue(osc::MessageFifo* queue) { oscBroadcastQueue_ = queue; }

    // EigenApi::Callback overrides
    void device(const char* dev, EigenApi::Callback::DeviceType dt, int rows, int cols, int ribbons, int pedals) override;
    void disconnect(const char* dev, EigenApi::Callback::DeviceType dt) override;
    void key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p, int r, int y) override;
    void breath(const char* dev, unsigned long long t, unsigned val) override;
    void strip(const char* dev, unsigned long long t, unsigned strip, unsigned val, bool a) override;
    void pedal(const char* dev, unsigned long long t, unsigned pedal, unsigned val) override;

private:
    void run() override;
    
    FirmwareReader fwReader_;
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
