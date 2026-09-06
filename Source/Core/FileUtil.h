#pragma once
#include <JuceHeader.h>
#include "Enums.h"

namespace ecm {

class FileUtil {
public:
    static void loadLayout(InstrumentType instrumentType, juce::ValueTree& rootState, juce::Component* parentComponent, std::function<void()> onFinished = nullptr);
    static void saveLayout(InstrumentType instrumentType, juce::ValueTree& rootState, juce::Component* parentComponent);

private:
    static juce::String getFileExtension(InstrumentType instrumentType);
    static juce::String getDeviceFolder(InstrumentType instrumentType);
};

} // namespace ecm
