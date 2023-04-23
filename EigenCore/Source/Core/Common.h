#pragma once
#include "Enums.h"
struct ConnectedDevice {
    const char *dev;
    EHDeviceType type = EHDeviceType::None;
    int assignedLEDColours[3][120]; // course, key
    bool activeKeys[3][120]; // course, key
};
extern std::list<ConnectedDevice> connectedDevices;
extern volatile std::atomic<bool> exitThreads;
extern std::atomic<bool> mapperConnected;
