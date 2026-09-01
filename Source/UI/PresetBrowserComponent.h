#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include "AppStyle.h"

class ECMapperAudioProcessor;

namespace ecm {

class PresetBrowserComponent : public juce::Component
{
public:
    enum class Mode
    {
        Browse,
        Save
    };

    PresetBrowserComponent(ECMapperAudioProcessor& processorToUse, Mode modeToUse);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int rowsPerColumn = 16;
    static constexpr int slotCount = 32;

    void refreshSlots();
    void handleSlotSelected(int slot);
    void requestDeleteSlot(int slot);
    void requestSave();
    void savePresetToSlot(int slot, const juce::String& name);

    ECMapperAudioProcessor& processor;
    Mode mode;
    int selectedSlot = 1;

    juce::Label headerLabel;
    juce::TextEditor presetNameEditor;
    juce::TextButton saveButton { "Save" };

    std::array<juce::TextButton, slotCount> slotButtons {};
    std::array<juce::TextButton, slotCount> deleteButtons {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserComponent)
};

} // namespace ecm
