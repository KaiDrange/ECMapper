#include <iostream>
#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/FileUtil.h"
#include "Core/LayoutChangeHandler.h"
#include "Core/LayoutWrapper.h"
#include "Core/OSCMessage.h"
#include "Core/PresetBankFileUtil.h"
#include "Core/SettingsWrapper.h"
#include "Core/ZoneWrapper.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

class SilentLogger : public juce::Logger {
public:
    void logMessage(const juce::String&) override {}
};

class DummyProcessor : public juce::AudioProcessor {
public:
    DummyProcessor()
        : juce::AudioProcessor(BusesProperties()) {
    }

    const juce::String getName() const override { return "LayoutLoadListenerTest"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

ecm::LayoutWrapper::LayoutKey makeNoteKey(int keyNo, int noteNumber) {
    ecm::LayoutWrapper::LayoutKey key;
    key.keyId = { 0, keyNo, ecm::InstrumentType::Alpha };
    key.keyType = ecm::EigenharpKeyType::Normal;
    key.keyColour = ecm::KeyColour::Off;
    key.zone = ecm::Zone::Zone1;
    key.keyMappingType = ecm::KeyMappingType::Note;
    key.mappingValue = juce::String(noteNumber);
    return key;
}

} // namespace

int main() {
    using namespace ecm;

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    osc::MessageFifo queue;
    LayoutChangeHandler handler(queue,
                                pluginState.state,
                                configLookups,
                                stateLock,
                                [] { return false; });
    pluginState.state.addListener(&handler);

    ZoneWrapper::setEnabled(InstrumentType::Alpha, Zone::Zone1, true, pluginState.state);
    configLookups[0].updateAll();

    bool ok = true;

    juce::ValueTree loadedRoot { "LoadedRoot" };
    auto loadedKey = makeNoteKey(0, 67);
    LayoutWrapper::setLayoutKey(loadedKey, loadedRoot);
    auto loadedLayout = LayoutWrapper::getLayoutTree(InstrumentType::Alpha, loadedRoot);
    ok &= expect(!loadedLayout.hasProperty(LayoutWrapper::id_ecMapperVersion),
                 "legacy layouts without an ECMapper version should still be supported");
    LayoutWrapper::getLayoutTree(InstrumentType::Alpha, pluginState.state).copyPropertiesAndChildrenFrom(loadedLayout, nullptr);
    ok &= expect(configLookups[0].keys[0][0].mapType == KeyMappingType::Note,
                 "layout import should immediately populate runtime key mapping without an extra edit");
    ok &= expect(configLookups[0].keys[0][0].notes[0] == 67,
                 "layout import should immediately refresh the imported note value");

    juce::ValueTree replacementRoot { "ReplacementRoot" };
    auto replacementKey = makeNoteKey(1, 71);
    LayoutWrapper::setLayoutKey(replacementKey, replacementRoot);
    auto replacementLayout = LayoutWrapper::getLayoutTree(InstrumentType::Alpha, replacementRoot);
    LayoutWrapper::getLayoutTree(InstrumentType::Alpha, pluginState.state).copyPropertiesAndChildrenFrom(replacementLayout, nullptr);

    ok &= expect(configLookups[0].keys[0][0].notes[0] == 0,
                 "replacing a layout should clear the old imported key mapping immediately");
    ok &= expect(configLookups[0].keys[0][1].mapType == KeyMappingType::Note,
                 "replacing a layout should immediately populate newly imported keys");
    ok &= expect(configLookups[0].keys[0][1].notes[0] == 71,
                 "replacing a layout should immediately refresh the new imported note value");

    auto persistedLayout = LayoutWrapper::createPersistentLayoutTree(InstrumentType::Alpha, replacementRoot);
    ok &= expect(persistedLayout.getProperty(LayoutWrapper::id_ecMapperVersion).toString() == ProjectInfo::versionString,
                 "saved layouts should include the current ECMapper version");

    const auto persistedXml = persistedLayout.createXml();
    ok &= expect(persistedXml != nullptr,
                 "layout export should produce XML");
    if (persistedXml != nullptr) {
        const auto roundTrippedLayout = juce::ValueTree::fromXml(*persistedXml);
        ok &= expect(roundTrippedLayout.isValid(),
                     "layout export should produce valid XML");
        ok &= expect(roundTrippedLayout.getProperty(LayoutWrapper::id_ecMapperVersion).toString() == ProjectInfo::versionString,
                     "serialized layout XML should preserve the ECMapper version");
    }

    juce::ValueTree legacyState { "LegacyState" };
    auto legacySettings = legacyState.getOrCreateChildWithName(SettingsWrapper::id_globalSettings, nullptr);
    legacySettings.setProperty(SettingsWrapper::id_lowerMPEVoiceCount, 9, nullptr);
    legacySettings.setProperty(SettingsWrapper::id_upperMPEPB, 24, nullptr);
    auto legacyDevice = legacyState.getOrCreateChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Alpha), nullptr);
    auto legacyLayout = legacyDevice.getOrCreateChildWithName(LayoutWrapper::id_layout, nullptr);
    auto legacyKey = legacyLayout.getOrCreateChildWithName(LayoutWrapper::id_key + juce::String("_0_2"), nullptr);
    legacyKey.setProperty(LayoutWrapper::id_keyType, (int)EigenharpKeyType::Normal, nullptr);
    legacyKey.setProperty(LayoutWrapper::id_keyColour, (int)KeyColour::Off, nullptr);
    legacyKey.setProperty(LayoutWrapper::id_zone, (int)Zone::Zone1, nullptr);
    legacyKey.setProperty(LayoutWrapper::id_keyMappingType, (int)KeyMappingType::Note, nullptr);
    legacyKey.setProperty(LayoutWrapper::id_mappingValue, "75", nullptr);

    SettingsWrapper::normalizeStateTree(legacyState);
    auto presetTree = SettingsWrapper::getPresetTree(legacyState);
    ok &= expect(legacyState.hasProperty(SettingsWrapper::id_ecMapperVersion),
                 "normalized root state should include an ECMapper version");
    ok &= expect(presetTree.hasProperty(SettingsWrapper::id_ecMapperVersion),
                 "normalized preset state should include an ECMapper version");
    ok &= expect(!legacyState.getChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Alpha)).isValid(),
                 "legacy root device nodes should move into the preset subtree");
    ok &= expect(presetTree.getChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Alpha)).isValid(),
                 "normalized preset should contain migrated device nodes");
    ok &= expect(!legacySettings.hasProperty(SettingsWrapper::id_lowerMPEVoiceCount),
                 "legacy preset properties should be removed from global settings during migration");
    ok &= expect(SettingsWrapper::getLowerMPEVoiceCount(legacyState) == 9,
                 "lower MPE voice count should migrate into the preset subtree");
    ok &= expect(SettingsWrapper::getUpperMPEPB(legacyState) == 24,
                 "upper MPE pitch bend should migrate into the preset subtree");
    ok &= expect(LayoutWrapper::getLayoutKey({ 0, 2, InstrumentType::Alpha }, legacyState).mappingValue == "75",
                 "migrated preset layouts should remain readable through the wrappers");

    auto persistentState = SettingsWrapper::createPersistentStateTree(pluginState.state);
    ok &= expect(persistentState.hasProperty(SettingsWrapper::id_ecMapperVersion),
                 "persistent full state should include a root ECMapper version");
    ok &= expect(persistentState.getChildWithName(SettingsWrapper::id_preset).hasProperty(SettingsWrapper::id_ecMapperVersion),
                 "persistent full state should include a preset ECMapper version");

    auto expectedLayoutsRoot = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("ECMapperLayouts");
    ok &= expect(FileUtil::getLayoutsRootDirectory() == expectedLayoutsRoot,
                 "layout root directory should resolve to the ECMapperLayouts folder in Documents");
    ok &= expect(juce::File::isAbsolutePath(FileUtil::getLayoutsRootDirectory().getFullPathName()),
                 "layout root directory should be absolute");
    ok &= expect(FileUtil::getLayoutDirectory(InstrumentType::Tau) == expectedLayoutsRoot.getChildFile("Tau"),
                 "device layout directories should resolve inside the ECMapperLayouts folder");

    juce::ValueTree presetBank("ECMapperPresetBank");
    juce::ValueTree exportedPreset("ECMapperPreset");
    exportedPreset.setProperty("slot", 2, nullptr);
    exportedPreset.setProperty("name", "Imported Slot", nullptr);
    juce::ValueTree exportedSnapshot("APVTS");
    auto presetBankKey = makeNoteKey(4, 81);
    LayoutWrapper::setLayoutKey(presetBankKey, exportedSnapshot);
    SettingsWrapper::setMidi2Mode(true, exportedSnapshot);
    exportedPreset.addChild(exportedSnapshot, -1, nullptr);
    presetBank.addChild(exportedPreset, -1, nullptr);

    auto presetBankExport = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("ecmapper_preset_bank_export", ".xml", false);
    ok &= expect(PresetBankFileUtil::writePresetBankFile(presetBankExport, presetBank),
                 "exporting a preset bank should succeed");

    auto importResult = PresetBankFileUtil::readPresetBankFile(presetBankExport);
    ok &= expect(importResult.presetBank.isValid(),
                 "importing an exported preset bank should succeed");
    auto normalizedPreset = importResult.presetBank.getChildWithProperty("slot", 2);
    auto normalizedSnapshot = normalizedPreset.getChildWithName(exportedSnapshot.getType());
    auto exportedPresetTree = normalizedSnapshot.getChildWithName(SettingsWrapper::id_preset);
    ok &= expect(importResult.presetBank.hasType("ECMapperPresetBank"),
                 "exported preset bank should use the preset bank root node");
    ok &= expect(exportedPresetTree.hasProperty(SettingsWrapper::id_ecMapperVersion),
                 "exported preset snapshots should preserve the preset version");
    ok &= expect(exportedPresetTree.getChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Alpha)).isValid(),
                 "exported preset snapshots should include preset device trees");
    ok &= expect(SettingsWrapper::getMidi2Mode(normalizedSnapshot),
                 "preset bank import should preserve preset-level settings");
    ok &= expect(LayoutWrapper::getLayoutKey({ 0, 4, InstrumentType::Alpha }, normalizedSnapshot).mappingValue == "81",
                 "imported preset bank should restore preset layout data");

    juce::ValueTree legacyPresetBank("ECMapperPresetBank");
    juce::ValueTree legacyPreset("ECMapperPreset");
    legacyPreset.setProperty("slot", 3, nullptr);
    legacyPreset.setProperty("name", "Legacy", nullptr);
    juce::ValueTree legacySnapshot("APVTS");
    auto legacyBankSettings = legacySnapshot.getOrCreateChildWithName(SettingsWrapper::id_globalSettings, nullptr);
    legacyBankSettings.setProperty(SettingsWrapper::id_midi2Mode, true, nullptr);
    auto legacyBankDevice = legacySnapshot.getOrCreateChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Alpha), nullptr);
    auto legacyBankLayout = legacyBankDevice.getOrCreateChildWithName(LayoutWrapper::id_layout, nullptr);
    auto legacyBankKey = legacyBankLayout.getOrCreateChildWithName(LayoutWrapper::id_key + juce::String("_0_5"), nullptr);
    legacyBankKey.setProperty(LayoutWrapper::id_keyType, (int)EigenharpKeyType::Normal, nullptr);
    legacyBankKey.setProperty(LayoutWrapper::id_keyColour, (int)KeyColour::Off, nullptr);
    legacyBankKey.setProperty(LayoutWrapper::id_zone, (int)Zone::Zone1, nullptr);
    legacyBankKey.setProperty(LayoutWrapper::id_keyMappingType, (int)KeyMappingType::Note, nullptr);
    legacyBankKey.setProperty(LayoutWrapper::id_mappingValue, "90", nullptr);
    legacyPreset.addChild(legacySnapshot, -1, nullptr);
    legacyPresetBank.addChild(legacyPreset, -1, nullptr);

    auto legacyPresetFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("ecmapper_legacy_preset_bank", ".xml", false);
    if (auto legacyXml = legacyPresetBank.createXml())
        legacyXml->writeTo(legacyPresetFile);

    auto legacyImportResult = PresetBankFileUtil::readPresetBankFile(legacyPresetFile);
    ok &= expect(legacyImportResult.presetBank.isValid(),
                 "importing a legacy preset bank should succeed");
    auto legacyImportedPreset = legacyImportResult.presetBank.getChildWithProperty("slot", 3);
    auto legacyImportedSnapshot = legacyImportedPreset.getChildWithName(legacySnapshot.getType());
    ok &= expect(legacyImportedSnapshot.getChildWithName(SettingsWrapper::id_preset).isValid(),
                 "legacy preset banks should migrate imported snapshots into the preset subtree");
    ok &= expect(SettingsWrapper::getMidi2Mode(legacyImportedSnapshot),
                 "legacy preset properties should migrate during preset bank import");
    ok &= expect(LayoutWrapper::getLayoutKey({ 0, 5, InstrumentType::Alpha }, legacyImportedSnapshot).mappingValue == "90",
                 "legacy preset layouts should migrate during preset bank import");

    auto invalidPresetBankFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("ecmapper_invalid_preset_bank", ".xml", false);
    juce::ValueTree invalidPresetBank("NotAPresetBank");
    if (auto invalidXml = invalidPresetBank.createXml())
        invalidXml->writeTo(invalidPresetBankFile);

    auto invalidImportResult = PresetBankFileUtil::readPresetBankFile(invalidPresetBankFile);
    ok &= expect(!invalidImportResult.presetBank.isValid(),
                 "invalid preset bank files should be rejected");

    presetBankExport.deleteFile();
    legacyPresetFile.deleteFile();
    invalidPresetBankFile.deleteFile();

    pluginState.state.removeListener(&handler);
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "LayoutLoadListenerTest passed" << std::endl;
    return 0;
}