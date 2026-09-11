#pragma once

#include <JuceHeader.h>

namespace ecm {

struct PresetBankImportResult {
    juce::ValueTree presetBank;
    bool wasStateBundle = false;
};

class PresetBankFileUtil {
public:
    static juce::ValueTree createExportTree(const juce::ValueTree& presetBankState);
    static bool writePresetBankFile(const juce::File& file, const juce::ValueTree& presetBankState);
    static PresetBankImportResult readPresetBankFile(const juce::File& file);
    static PresetBankImportResult readPresetBankTree(const juce::ValueTree& tree);
    static void normalizePresetBankState(juce::ValueTree& presetBankState);
};

} // namespace ecm