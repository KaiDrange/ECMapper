#pragma once
#include <JuceHeader.h>
#include "../Core/HardwareService.h"

namespace ecm {

class CorePage : public juce::Component, 
                 public HardwareService::Listener,
                 private juce::Timer {
public:
    CorePage(HardwareService& hardwareService, juce::ValueTree& state);
    ~CorePage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void deviceListChanged() override;
    void timerCallback() override;

private:
    void updateDeviceList();
    void updateClockControls();

    HardwareService& hardwareService_;
    juce::ValueTree& state_;
    
    juce::ComboBox roleCombo;
    juce::Label roleLabel;
    
    juce::Label clientIpLabel;
    juce::TextEditor clientIpInput;
    juce::Label clientPortLabel;
    juce::TextEditor clientPortInput;
    
    juce::GroupComponent audioGroup { "audioOutput", "Audio output - all devices" };
    juce::Slider metronomeVolume;
    juce::Slider audioInputVolume;
    juce::Label metronomeLabel { "", "Metronome volume" };
    juce::Label audioInputLabel { "", "Audio input volume" };
    juce::GroupComponent clockGroup { "clock", "Clock and transport" };
    juce::ToggleButton midiClockIn { "MIDI Clock In" };
    juce::ToggleButton midiClockMaster { "MIDI Clock Master" };
    juce::ToggleButton abletonLink { "Ableton Link" };
    juce::Label bpmLabel { "", "BPM" };
    juce::Slider bpmInput;
    juce::Label timeSignatureLabel { "", "Time signature" };
    juce::ComboBox timeSignature;
    juce::TextButton startButton { "Start" };
    juce::TextButton stopButton { "Stop" };
    juce::ValueTree clockSettings;
    bool transportRunning = false; // UI state only; never restore playback on launch.

    juce::Label devicesLabel { "", "Connected hardware" };
    juce::Label emptyDevicesLabel;
    juce::Viewport deviceViewport;
    juce::Component deviceContent;

    struct TargetRow {
        std::unique_ptr<juce::Label> ipLabel;
        std::unique_ptr<juce::TextEditor> ipInput;
        std::unique_ptr<juce::Label> portLabel;
        std::unique_ptr<juce::TextEditor> portInput;
        std::unique_ptr<juce::TextButton> ledToggle;
        std::unique_ptr<juce::TextButton> addButton;
        std::unique_ptr<juce::TextButton> removeButton;
    };

    struct DeviceRow {
        std::string dev;
        std::unique_ptr<juce::GroupComponent> card;
        std::unique_ptr<juce::Slider> headphoneGain;
        std::unique_ptr<juce::Label> headphoneGainLabel;
        std::unique_ptr<juce::TextButton> headphoneEnabled;
        std::unique_ptr<juce::ImageComponent> statusLed;
        std::unique_ptr<juce::Label> nameLabel;
        std::unique_ptr<juce::ComboBox> modeCombo;
        std::vector<std::unique_ptr<TargetRow>> targets;
        std::unique_ptr<juce::TextButton> emptyAddButton;
    };
    
    std::vector<std::unique_ptr<DeviceRow>> deviceRows_;

    juce::Image ledGreen;
    [[maybe_unused]] juce::Image ledOff;
    juce::Image ledRed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorePage)
};

} // namespace ecm
