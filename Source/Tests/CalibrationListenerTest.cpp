#include <iostream>

#include "Core/SettingsWrapper.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

struct CountingListener : juce::ValueTree::Listener {
    int propertyChanges = 0;

    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override {
        ++propertyChanges;
    }
};

} // namespace

int main() {
    using namespace ecm;

    juce::ValueTree root { "Root" };

    CountingListener listener;
    root.addListener(&listener);

    SettingsWrapper::setCalibrationValue(InstrumentType::Alpha, SettingsWrapper::id_rollSensitivity, 2.1f, root);
    bool ok = expect(listener.propertyChanges > 0,
                     "listener should receive live calibration updates without requiring a restart");

    root.removeListener(&listener);
    listener.propertyChanges = 0;
    SettingsWrapper::setCalibrationValue(InstrumentType::Alpha, SettingsWrapper::id_rollSensitivity, 1.4f, root);
    ok &= expect(listener.propertyChanges == 0,
                 "listener removal should detach nested calibration listeners too");

    if (!ok)
        return 1;

    std::cout << "CalibrationListenerTest passed" << std::endl;
    return 0;
}