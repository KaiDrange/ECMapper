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
    PresetBrowserComponent(ECMapperAudioProcessor& processorToUse);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int rowsPerColumn = 16;
    static constexpr int slotCount = 32;

    void refreshSlots();
    void handleSlotSelected(int slot);
    void requestDeleteSlot(int slot);
    void requestSaveToSlot(int slot);
    void savePresetToSlot(int slot, const juce::String& name);
    void closeDialog();

    ECMapperAudioProcessor& processor;
    int selectedSlot = 1;

    juce::Label headerLabel;

    std::array<juce::TextButton, slotCount> slotButtons {};
    std::array<juce::TextButton, slotCount> deleteButtons {};
    std::array<juce::TextButton, slotCount> saveButtons {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserComponent)
};

} // namespace ecm
