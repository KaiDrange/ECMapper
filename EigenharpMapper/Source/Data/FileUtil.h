#pragma once
#include <JuceHeader.h>
#include "../Models/Layout.h"
#include "../Models/Enums.h"


class FileUtil {
public:
    static juce::String openMapping(DeviceType::DeviceType instrumentType);
    static bool saveMapping(juce::ValueTree valueTree, DeviceType::DeviceType instrumentType);

private:
    static juce::String getFileExtension(DeviceType::DeviceType instrumentType);
};
