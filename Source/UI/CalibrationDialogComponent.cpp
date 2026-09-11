#include "CalibrationDialogComponent.h"
#include "AppStyle.h"

namespace ecm {

namespace {

constexpr float calibrationThresholdMin = 0.0f;
constexpr float calibrationThresholdMax = 0.2f;

}

CalibrationDialogComponent::DeviceCalibrationPanel::DeviceCalibrationPanel(InstrumentType type, juce::ValueTree& state, juce::Slider::Listener* listener)
    : type(type), state(state)
{
    auto setupSlider = [this, listener](juce::Slider& s, juce::Label& l, const juce::String& name, float min, float max, float step) {
        l.setText(name, juce::dontSendNotification);
        addAndMakeVisible(l);
        s.setRange(min, max, step);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        s.addListener(listener);
        addAndMakeVisible(s);
    };

    setupSlider(breathThresholdSlider, breathThresholdLabel, "Breath Threshold", calibrationThresholdMin, calibrationThresholdMax, 0.001f);
    setupSlider(breathSensitivitySlider, breathSensitivityLabel, "Breath Sensitivity", 0.5f, 3.0f, 0.01f);
    setupSlider(stripThresholdSlider, stripThresholdLabel, "Strip Threshold", calibrationThresholdMin, calibrationThresholdMax, 0.001f);
    setupSlider(stripSensitivitySlider, stripSensitivityLabel, "Strip Sensitivity", 0.5f, 3.0f, 0.01f);
    setupSlider(yawSensitivitySlider, yawSensitivityLabel, "Yaw Sensitivity", 0.5f, 3.0f, 0.01f);
    setupSlider(rollSensitivitySlider, rollSensitivityLabel, "Roll Sensitivity", 0.5f, 3.0f, 0.01f);
    setupSlider(pressureSensitivitySlider, pressureSensitivityLabel, "Pressure Sensitivity", 0.5f, 3.0f, 0.01f);

    invertStripDirectionButton.setButtonText("Invert strip direction");
    invertStripDirectionButton.setClickingTogglesState(true);
    invertStripDirectionButton.onClick = [this]() {
        SettingsWrapper::setCalibrationBool(this->type, SettingsWrapper::id_invertStripDirection, invertStripDirectionButton.getToggleState(), this->state);
    };
    addAndMakeVisible(invertStripDirectionButton);

    resetButton.setButtonText("Reset to Defaults");
    resetButton.onClick = [this]() {
        SettingsWrapper::resetCalibration(this->type, this->state);
        updateValues();
    };
    addAndMakeVisible(resetButton);

    updateValues();
}

void CalibrationDialogComponent::DeviceCalibrationPanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    int h = 30;
    int gap = 5;

    auto layoutRow = [&](juce::Label& l, juce::Slider& s, int y) {
        l.setBounds(area.getX(), y, 120, h);
        s.setBounds(area.getX() + 120, y, area.getWidth() - 120, h);
    };

    layoutRow(breathThresholdLabel, breathThresholdSlider, area.getY());
    layoutRow(breathSensitivityLabel, breathSensitivitySlider, area.getY() + (h + gap));
    layoutRow(stripThresholdLabel, stripThresholdSlider, area.getY() + 2 * (h + gap));
    layoutRow(stripSensitivityLabel, stripSensitivitySlider, area.getY() + 3 * (h + gap));
    layoutRow(yawSensitivityLabel, yawSensitivitySlider, area.getY() + 4 * (h + gap));
    layoutRow(rollSensitivityLabel, rollSensitivitySlider, area.getY() + 5 * (h + gap));
    layoutRow(pressureSensitivityLabel, pressureSensitivitySlider, area.getY() + 6 * (h + gap));

    invertStripDirectionButton.setBounds(area.getX(), area.getY() + 7 * (h + gap), area.getWidth(), h);

    resetButton.setBounds(area.getX(), area.getBottom() - 30, area.getWidth(), 30);
}

void CalibrationDialogComponent::DeviceCalibrationPanel::updateValues()
{
    float breathD = (type == InstrumentType::Pico) ? 0.125f : 0.03125f;
    float stripTD = (type == InstrumentType::Pico) ? 0.12f : 0.0366f;
    float stripGD = (type == InstrumentType::Pico) ? 1.2f : 1.3f;

    breathThresholdSlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_breathThreshold, breathD, state), juce::dontSendNotification);
    breathSensitivitySlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_breathSensitivity, 1.7f, state), juce::dontSendNotification);
    stripThresholdSlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_stripThreshold, stripTD, state), juce::dontSendNotification);
    stripSensitivitySlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_stripSensitivity, stripGD, state), juce::dontSendNotification);
    yawSensitivitySlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_yawSensitivity, 1.7f, state), juce::dontSendNotification);
    rollSensitivitySlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_rollSensitivity, 1.7f, state), juce::dontSendNotification);
    pressureSensitivitySlider.setValue(SettingsWrapper::getCalibrationValue(type, SettingsWrapper::id_pressureSensitivity, 1.7f, state), juce::dontSendNotification);
    invertStripDirectionButton.setToggleState(SettingsWrapper::getCalibrationBool(type, SettingsWrapper::id_invertStripDirection, false, state), juce::dontSendNotification);
}

CalibrationDialogComponent::CalibrationDialogComponent(juce::ValueTree& state)
    : state(state),
      tabs(juce::TabbedButtonBar::TabsAtTop)
{
    alphaPanel = std::make_unique<DeviceCalibrationPanel>(InstrumentType::Alpha, state, this);
    tauPanel = std::make_unique<DeviceCalibrationPanel>(InstrumentType::Tau, state, this);
    picoPanel = std::make_unique<DeviceCalibrationPanel>(InstrumentType::Pico, state, this);

    tabs.addTab("Alpha", Style::tabColour(1), alphaPanel.get(), false);
    tabs.addTab("Tau", Style::tabColour(2), tauPanel.get(), false);
    tabs.addTab("Pico", Style::tabColour(3), picoPanel.get(), false);
    tabs.setTabBarDepth(34);
    tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    tabs.getTabbedButtonBar().setColour(juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    
    addAndMakeVisible(tabs);

    closeButton.setButtonText("Close");
    closeButton.onClick = [this]() { closeDialog(); };
    addAndMakeVisible(closeButton);

    setSize(450, 400);
}

void CalibrationDialogComponent::paint(juce::Graphics& g)
{
    g.fillAll(Style::background());

    auto header = getLocalBounds().removeFromTop(tabs.getTabBarDepth() + 8);
    g.setColour(Style::background().interpolatedWith(Style::surface(), 0.24f));
    g.fillRect(header);

    g.setColour(Style::accent().withAlpha(0.70f));
    g.fillRect(header.removeFromTop(3));
}

void CalibrationDialogComponent::resized()
{
    auto area = getLocalBounds();
    closeButton.setBounds(area.removeFromBottom(40).reduced(10, 5));
    
    // Position tabs so the buttons are vertically centered in the header area (excluding accent line)
    // The header in paint is tabBarDepth + 8. Accent line is 3. 
    // So there's 5px extra. Let's just give it a 4px top offset.
    tabs.setBounds(area.withTrimmedTop(4));
}

void CalibrationDialogComponent::sliderValueChanged(juce::Slider* slider)
{
    DeviceCalibrationPanel* panels[] = { alphaPanel.get(), tauPanel.get(), picoPanel.get() };
    DeviceCalibrationPanel* panel = nullptr;

    for (auto* p : panels) {
        if (slider == &p->breathThresholdSlider ||
            slider == &p->breathSensitivitySlider ||
            slider == &p->stripThresholdSlider ||
            slider == &p->stripSensitivitySlider ||
            slider == &p->yawSensitivitySlider ||
            slider == &p->rollSensitivitySlider ||
            slider == &p->pressureSensitivitySlider)
        {
            panel = p;
            break;
        }
    }

    if (panel == nullptr) return;

    if (slider == &panel->breathThresholdSlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_breathThreshold, (float)slider->getValue(), state);
    else if (slider == &panel->breathSensitivitySlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_breathSensitivity, (float)slider->getValue(), state);
    else if (slider == &panel->stripThresholdSlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_stripThreshold, (float)slider->getValue(), state);
    else if (slider == &panel->stripSensitivitySlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_stripSensitivity, (float)slider->getValue(), state);
    else if (slider == &panel->yawSensitivitySlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_yawSensitivity, (float)slider->getValue(), state);
    else if (slider == &panel->rollSensitivitySlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_rollSensitivity, (float)slider->getValue(), state);
    else if (slider == &panel->pressureSensitivitySlider)
        SettingsWrapper::setCalibrationValue(panel->type, SettingsWrapper::id_pressureSensitivity, (float)slider->getValue(), state);
}

void CalibrationDialogComponent::closeDialog()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(0);
}

} // namespace ecm
