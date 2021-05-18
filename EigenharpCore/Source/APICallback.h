#pragma once

#include <JuceHeader.h>
#include <unistd.h>
#include <signal.h>
#include <eigenapi.h>
#include <iostream>
#include <thread>
#include "OSCCommunication.h"
#include "Enums.h"

struct ConnectedDevice {
    const char *dev;
    EHDeviceType type = EHDeviceType::None;
};
extern std::list<ConnectedDevice> connectedDevices;

class APICallback: public EigenApi::Callback {
public:
    APICallback(EigenApi::Eigenharp& eh, OSC::OSCMessageFifo *sendQueue);
    virtual void device(const char* dev, DeviceType dt, int rows, int cols, int ribbons, int pedals);
    virtual void key(const char* dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p, int r, int y);
    virtual void breath(const char* dev, unsigned long long t, unsigned val);
    virtual void strip(const char* dev, unsigned long long t, unsigned strip, unsigned val);
    virtual void pedal(const char* dev, unsigned long long t, unsigned pedal, unsigned val);
    
    EigenApi::Eigenharp& eh_;

private:
    OSC::OSCMessageFifo *sendQueue;
    EHDeviceType getTypeFromDev(const char* dev) const;
};
