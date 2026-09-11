#define private public
#include "UI/CalibrationDialogComponent.h"
#undef private

#include <cmath>
#include <iostream>

namespace {

constexpr double sliderTolerance = 1.0e-5;

bool expectNear(double actual, double expected, const char* message) {
    if (std::abs(actual - expected) > sliderTolerance) {
        std::cerr << message << " (expected " << expected << ", got " << actual << ")" << std::endl;
        return false;
    }

    return true;
}

bool expectStep(const juce::Slider& slider, const char* name) {
    return expectNear(slider.getInterval(), 0.001, name);
}

bool expectThresholdRange(const juce::Slider& slider, const char* name) {
    bool ok = expectNear(slider.getMinimum(), 0.0, name);
    ok &= expectNear(slider.getMaximum(), 0.2, name);
    ok &= expectStep(slider, name);
    return ok;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main() {
    using namespace ecm;

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::ValueTree root { "Root" };
    CalibrationDialogComponent dialog(root);

    bool ok = true;
    ok &= expectThresholdRange(dialog.alphaPanel->breathThresholdSlider, "alpha breath threshold range should be 0.0 to 0.2");
    ok &= expectThresholdRange(dialog.alphaPanel->stripThresholdSlider, "alpha strip threshold range should be 0.0 to 0.2");
    ok &= expectThresholdRange(dialog.tauPanel->breathThresholdSlider, "tau breath threshold range should be 0.0 to 0.2");
    ok &= expectThresholdRange(dialog.tauPanel->stripThresholdSlider, "tau strip threshold range should be 0.0 to 0.2");
    ok &= expectThresholdRange(dialog.picoPanel->breathThresholdSlider, "pico breath threshold range should be 0.0 to 0.2");
    ok &= expectThresholdRange(dialog.picoPanel->stripThresholdSlider, "pico strip threshold range should be 0.0 to 0.2");
    ok &= expect(!dialog.alphaPanel->invertStripDirectionButton.getToggleState(), "alpha invert strip direction should default to off");
    ok &= expect(!dialog.tauPanel->invertStripDirectionButton.getToggleState(), "tau invert strip direction should default to off");
    ok &= expect(!dialog.picoPanel->invertStripDirectionButton.getToggleState(), "pico invert strip direction should default to off");

    dialog.alphaPanel->invertStripDirectionButton.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    ok &= expect(dialog.alphaPanel->invertStripDirectionButton.getToggleState(), "alpha invert strip direction should toggle on");
    ok &= expect(SettingsWrapper::getCalibrationBool(InstrumentType::Alpha, SettingsWrapper::id_invertStripDirection, false, root),
                 "alpha invert strip direction should persist in calibration state");

    CalibrationDialogComponent reopenedDialog(root);
    ok &= expect(reopenedDialog.alphaPanel->invertStripDirectionButton.getToggleState(),
                 "alpha invert strip direction should restore from calibration state");

    reopenedDialog.alphaPanel->resetButton.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    ok &= expect(!reopenedDialog.alphaPanel->invertStripDirectionButton.getToggleState(),
                 "alpha invert strip direction should reset to off");
    ok &= expect(!SettingsWrapper::getCalibrationBool(InstrumentType::Alpha, SettingsWrapper::id_invertStripDirection, false, root),
                 "alpha invert strip direction should be cleared by reset");

    if (!ok)
        return 1;

    std::cout << "CalibrationDialogThresholdRangeTest passed" << std::endl;
    return 0;
}