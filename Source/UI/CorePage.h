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

    HardwareService& hardwareService_;
    juce::ValueTree& state_;
    
    juce::ComboBox roleCombo;
    juce::Label roleLabel;
    
    juce::Label clientIpLabel;
    juce::TextEditor clientIpInput;
    juce::Label clientPortLabel;
    juce::TextEditor clientPortInput;
    
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
