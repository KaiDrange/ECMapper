#include <JuceHeader.h>

#include "Core/HardwareService.h"
#include "Core/SettingsWrapper.h"

#include <iostream>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool verifyHostStartupDefaultsLocalModeForSavedTransmitDevice()
{
    juce::ValueTree rootState("ECMapperState");

    ecm::ConnectedDevice savedDevice;
    savedDevice.dev = "alpha-1";
    savedDevice.type = ecm::InstrumentType::Alpha;
    savedDevice.mode = ecm::DeviceMode::TransmitOSC;
    savedDevice.oscTargets.push_back({"127.0.0.1", 12130, true});
    ecm::SettingsWrapper::saveDeviceSettings(savedDevice, rootState);

    ecm::ConnectedDevice restoredDevice;
    restoredDevice.dev = "alpha-1";
    restoredDevice.type = ecm::InstrumentType::Alpha;
    ecm::SettingsWrapper::loadDeviceSettings(restoredDevice, rootState);

    bool ok = true;
    ok &= expect(restoredDevice.mode == ecm::DeviceMode::TransmitOSC,
                 "test setup should restore the persisted transmit mode before host sanitization");
    ok &= expect(ecm::HardwareService::sanitizeLocalDeviceModeForAppRole(ecm::AppRole::Host, restoredDevice.mode)
                     == ecm::DeviceMode::Local,
                 "host startup should default a saved local transmit device back to Local mode");
    ok &= expect(!restoredDevice.oscTargets.empty(),
                 "sanitizing the startup mode should not discard saved OSC targets");

    return ok;
}

bool verifyClientRoleStillForcesReceiveMode()
{
    bool ok = true;
    ok &= expect(ecm::HardwareService::sanitizeLocalDeviceModeForAppRole(ecm::AppRole::Client, ecm::DeviceMode::Local)
                     == ecm::DeviceMode::ReceiveOSC,
                 "client role should still force local devices into Receive mode");
    ok &= expect(ecm::HardwareService::sanitizeLocalDeviceModeForAppRole(ecm::AppRole::Client, ecm::DeviceMode::TransmitOSC)
                     == ecm::DeviceMode::ReceiveOSC,
                 "client role should ignore saved transmit mode for local devices");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= verifyHostStartupDefaultsLocalModeForSavedTransmitDevice();
    ok &= verifyClientRoleStillForcesReceiveMode();

    if (!ok)
        return 1;

    std::cout << "HardwareServiceModeSanitizationTest passed" << std::endl;
    return 0;
}