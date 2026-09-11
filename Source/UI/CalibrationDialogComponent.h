#pragma once
#include <JuceHeader.h>
#include "../Core/Enums.h"
#include "../Core/SettingsWrapper.h"

namespace ecm {

class CalibrationDialogComponent : public juce::Component,
                                   public juce::Slider::Listener
{
public:
    CalibrationDialogComponent(juce::ValueTree& state);
    ~CalibrationDialogComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;

private:
    void updateSliders(InstrumentType type);
    void resetToDefaults(InstrumentType type);
    void closeDialog();

    struct DeviceCalibrationPanel : public juce::Component
    {
        DeviceCalibrationPanel(InstrumentType type, juce::ValueTree& state, juce::Slider::Listener* listener);
        void resized() override;
        void updateValues();

        InstrumentType type;
        juce::ValueTree& state;

        juce::Slider breathThresholdSlider;
        juce::Slider breathSensitivitySlider;
        juce::Slider stripThresholdSlider;
        juce::Slider stripSensitivitySlider;
        juce::Slider yawSensitivitySlider;
        juce::Slider rollSensitivitySlider;
        juce::Slider pressureSensitivitySlider;
        juce::ToggleButton invertStripDirectionButton;

        juce::Label breathThresholdLabel;
        juce::Label breathSensitivityLabel;
        juce::Label stripThresholdLabel;
        juce::Label stripSensitivityLabel;
        juce::Label yawSensitivityLabel;
        juce::Label rollSensitivityLabel;
        juce::Label pressureSensitivityLabel;
        
        juce::TextButton resetButton;
    };

    juce::ValueTree& state;
    juce::TabbedComponent tabs;
    std::unique_ptr<DeviceCalibrationPanel> alphaPanel;
    std::unique_ptr<DeviceCalibrationPanel> tauPanel;
    std::unique_ptr<DeviceCalibrationPanel> picoPanel;
    
    juce::TextButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CalibrationDialogComponent)
};

} // namespace ecm
