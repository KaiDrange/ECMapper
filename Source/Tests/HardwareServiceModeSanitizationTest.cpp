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

bool verifyHostStartupPreservesSavedTransmitDevice()
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
                     == ecm::DeviceMode::TransmitOSC,
                 "host startup must preserve a saved Transmit mode");
    ok &= expect(ecm::HardwareService::sanitizeLocalDeviceModeForAppRole(ecm::AppRole::Host, ecm::DeviceMode::Local)
                     == ecm::DeviceMode::Local,
                 "host startup must preserve Local mode");
    ok &= expect(ecm::HardwareService::sanitizeLocalDeviceModeForAppRole(ecm::AppRole::Host, ecm::DeviceMode::ReceiveOSC)
                     == ecm::DeviceMode::Local,
                 "host local hardware must not use Receive mode");
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

bool verifyStandaloneClockWithoutHardware()
{
    // Match standalone runtime detection without opening USB devices or a window.
    const auto previousFactory = juce::JUCEApplicationBase::createInstance;
    juce::JUCEApplicationBase::createInstance = []() -> juce::JUCEApplicationBase* { return nullptr; };
    bool ok = true;
    {
        ecm::osc::MessageFifo input;
        ecm::osc::MessageFifo output;
        ecm::HardwareService service(input, output);
        juce::ValueTree state("ECMapperState");
        ecm::SettingsWrapper::getClockSettings(state).setProperty(ecm::SettingsWrapper::id_clockSource, "midiMaster", nullptr);
        service.prepareMetronome(44100, state);
        ok &= expect(!service.supportsLocalHardware() && service.getAppRole() == ecm::AppRole::Client,
                     "clock-only regression must exercise a build without local hardware support");
        ok &= expect(service.startMetronome().isEmpty() && service.isMetronomePlaying(),
                     "standalone master must start without Alpha/Tau, including Client mode at 44.1 kHz");
        juce::MidiBuffer clock;
        service.processMetronome(1024, false, nullptr, &clock);
        ok &= expect(clock.getNumEvents() == 3 && (*clock.begin()).getMessage().isMidiStart(),
                     "hardware-free service must generate Start and clock pulses");
        service.stopMetronome();
        clock.clear();
        service.processMetronome(128, false, nullptr, &clock);
        ok &= expect(clock.getNumEvents() == 1 && (*clock.begin()).getMessage().isMidiStop(),
                     "hardware-free service must generate Stop");
    }
    juce::JUCEApplicationBase::createInstance = previousFactory;
    return ok;
}

bool verifyPluginClientMetronomeAudioOutput()
{
    bool ok = true;
    ok &= expect(!juce::JUCEApplicationBase::isStandaloneApp(),
                 "client audio-output regression must use plugin runtime detection");
    ecm::osc::MessageFifo input, output;
    ecm::HardwareService service(input, output);
    juce::ValueTree state("ECMapperState");
    ecm::SettingsWrapper::setAppRole(ecm::AppRole::Client, state);
    auto settings = ecm::SettingsWrapper::getAudioOutputSettings(state);
    settings.setProperty(ecm::SettingsWrapper::id_metronomeRoute, 2, nullptr);
    service.prepareMetronome(48000, state);
    ok &= expect(service.getAppRole() == ecm::AppRole::Client && service.startMetronome().isEmpty(),
                 "plugin client must start its own audio-output metronome without local hardware");
    juce::AudioBuffer<float> audio(2, 128);
    audio.clear();
    service.processMetronome(128, false, nullptr, nullptr, &audio, 0, 2);
    ok &= expect(service.isMetronomePlaying() && audio.getMagnitude(0, 128) > 0.0f,
                 "plugin client must render audible clicks through its own audio output");
    service.stopMetronome();
    audio.clear();
    service.processMetronome(128, false, nullptr, nullptr, &audio, 0, 2);
    ok &= expect(!service.isMetronomePlaying() && audio.getMagnitude(0, 128) == 0.0f,
                 "stopping the plugin client must silence its local metronome");
    settings.setProperty(ecm::SettingsWrapper::id_metronomeRoute, 3, nullptr);
    service.updateAudioSettings(state);
    ok &= expect(static_cast<int>(settings.getProperty(ecm::SettingsWrapper::id_metronomeRoute)) == 2
                     && service.startMetronome().isEmpty(),
                 "client Both route must retain audio output while removing headphones");
    service.stopMetronome();
    settings.setProperty(ecm::SettingsWrapper::id_metronomeRoute, 1, nullptr);
    service.updateAudioSettings(state);
    ok &= expect(static_cast<int>(settings.getProperty(ecm::SettingsWrapper::id_metronomeRoute)) == 4
                     && service.startMetronome().isNotEmpty(),
                 "client Headphones route must become None and not enable local headphones");
    return ok;
}

bool verifyTauButtonsUseSeparateCourseFromTauPercussion()
{
    bool ok = true;
    ok &= expect(ecm::HardwareService::getButtonCourseForInstrument(ecm::InstrumentType::Tau) == 2,
                 "Tau round buttons should route on course 2 so they do not collide with Tau percussion keys");
    ok &= expect(ecm::HardwareService::getButtonCourseForInstrument(ecm::InstrumentType::Pico) == 1,
                 "Pico buttons should keep routing on course 1");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= verifyHostStartupPreservesSavedTransmitDevice();
    ok &= verifyClientRoleStillForcesReceiveMode();
    ok &= verifyTauButtonsUseSeparateCourseFromTauPercussion();
    ok &= verifyStandaloneClockWithoutHardware();
    ok &= verifyPluginClientMetronomeAudioOutput();

    if (!ok)
        return 1;

    std::cout << "HardwareServiceModeSanitizationTest passed" << std::endl;
    return 0;
}
