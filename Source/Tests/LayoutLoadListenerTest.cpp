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

ecm::LayoutWrapper::LayoutKey makeTauKey(int course,
                                         int keyNo,
                                         ecm::EigenharpKeyType keyType,
                                         ecm::KeyMappingType mappingType,
                                         juce::String mappingValue,
                                         ecm::Zone zone = ecm::Zone::Zone1) {
    ecm::LayoutWrapper::LayoutKey key;
    key.keyId = { course, keyNo, ecm::InstrumentType::Tau };
    key.keyType = keyType;
    key.keyColour = ecm::KeyColour::Off;
    key.zone = zone;
    key.keyMappingType = mappingType;
    key.mappingValue = std::move(mappingValue);
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

    juce::ValueTree duplicateState { "DuplicateState" };
    auto presetTreeWithLoadedLayout = SettingsWrapper::getPresetTree(duplicateState);
    auto loadedPresetDevice = presetTreeWithLoadedLayout.getOrCreateChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Pico), nullptr);
    auto loadedPresetLayout = loadedPresetDevice.getOrCreateChildWithName(LayoutWrapper::id_layout, nullptr);
    auto loadedPresetKey = loadedPresetLayout.getOrCreateChildWithName(LayoutWrapper::id_key + juce::String("_0_3"), nullptr);
    loadedPresetKey.setProperty(LayoutWrapper::id_keyType, (int)EigenharpKeyType::Normal, nullptr);
    loadedPresetKey.setProperty(LayoutWrapper::id_keyColour, (int)KeyColour::Off, nullptr);
    loadedPresetKey.setProperty(LayoutWrapper::id_zone, (int)Zone::Zone1, nullptr);
    loadedPresetKey.setProperty(LayoutWrapper::id_keyMappingType, (int)KeyMappingType::Note, nullptr);
    loadedPresetKey.setProperty(LayoutWrapper::id_mappingValue, "67", nullptr);

    auto staleLegacyDevice = duplicateState.getOrCreateChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Pico), nullptr);
    auto staleLegacyLayout = staleLegacyDevice.getOrCreateChildWithName(LayoutWrapper::id_layout, nullptr);
    auto staleLegacyKey = staleLegacyLayout.getOrCreateChildWithName(LayoutWrapper::id_key + juce::String("_0_3"), nullptr);
    staleLegacyKey.setProperty(LayoutWrapper::id_keyType, (int)EigenharpKeyType::Normal, nullptr);
    staleLegacyKey.setProperty(LayoutWrapper::id_keyColour, (int)KeyColour::Off, nullptr);
    staleLegacyKey.setProperty(LayoutWrapper::id_zone, (int)Zone::Zone1, nullptr);
    staleLegacyKey.setProperty(LayoutWrapper::id_keyMappingType, (int)KeyMappingType::Note, nullptr);
    staleLegacyKey.setProperty(LayoutWrapper::id_mappingValue, "0", nullptr);

    SettingsWrapper::setLowerMPEPB(11, duplicateState);
    ok &= expect(LayoutWrapper::getLayoutKey({ 0, 3, InstrumentType::Pico }, duplicateState).mappingValue == "67",
                 "normalizing duplicate legacy device trees should preserve the already-loaded preset layout mapping");
    ok &= expect(!duplicateState.getChildWithName(LayoutWrapper::id_device + juce::String((int)InstrumentType::Pico)).isValid(),
                 "normalizing duplicate legacy device trees should still remove the legacy root device node");

    juce::ValueTree tauCurrentRoot { "TauCurrentRoot" };
    auto tauPercKey = makeTauKey(1, 0, EigenharpKeyType::Perc, KeyMappingType::Note, "48", Zone::Zone3);
    LayoutWrapper::setLayoutKey(tauPercKey, tauCurrentRoot);
    auto resolvedTauPercKey = LayoutWrapper::getLayoutKey({ 1, 0, InstrumentType::Tau }, tauCurrentRoot);
    ok &= expect(resolvedTauPercKey.keyId.course == 1 && resolvedTauPercKey.keyId.keyNo == 0,
                 "Tau percussion keys should use course 1 with zero-based numbering");
    ok &= expect(resolvedTauPercKey.mappingValue == "48",
                 "Tau percussion mappings should round-trip with the current numbering");

    auto tauButtonKey = makeTauKey(2, 7, EigenharpKeyType::Button, KeyMappingType::None, {});
    LayoutWrapper::setLayoutKey(tauButtonKey, tauCurrentRoot);
    auto resolvedTauButtonKey = LayoutWrapper::getLayoutKey({ 2, 7, InstrumentType::Tau }, tauCurrentRoot);
    ok &= expect(resolvedTauButtonKey.keyId.course == 2 && resolvedTauButtonKey.keyId.keyNo == 7,
                 "Tau buttons should use course 2 with zero-based numbering");
    ok &= expect(resolvedTauButtonKey.keyType == EigenharpKeyType::Button,
                 "Tau buttons should keep their button type with the current numbering");

    auto tauCurrentPercOverlapCandidate = makeTauKey(1, 5, EigenharpKeyType::Perc, KeyMappingType::Note, "53", Zone::Zone3);
    LayoutWrapper::setLayoutKey(tauCurrentPercOverlapCandidate, tauCurrentRoot);
    auto tauCurrentButtonOverlapCandidate = makeTauKey(2, 0, EigenharpKeyType::Button, KeyMappingType::None, {}, Zone::Zone1);
    LayoutWrapper::setLayoutKey(tauCurrentButtonOverlapCandidate, tauCurrentRoot);
    auto resolvedTauCurrentPercOverlapCandidate = LayoutWrapper::getLayoutKey({ 1, 5, InstrumentType::Tau }, tauCurrentRoot);
    auto resolvedTauCurrentButtonOverlapCandidate = LayoutWrapper::getLayoutKey({ 2, 0, InstrumentType::Tau }, tauCurrentRoot);
    ok &= expect(resolvedTauCurrentPercOverlapCandidate.keyId.course == 1
                 && resolvedTauCurrentPercOverlapCandidate.keyId.keyNo == 5
                 && resolvedTauCurrentPercOverlapCandidate.zone == Zone::Zone3,
                 "Tau percussion key 6 should keep its own course 1 entry");
    ok &= expect(resolvedTauCurrentButtonOverlapCandidate.keyId.course == 2
                 && resolvedTauCurrentButtonOverlapCandidate.keyId.keyNo == 0
                 && resolvedTauCurrentButtonOverlapCandidate.zone == Zone::Zone1,
                 "Tau round button 1 should stay separate from Tau percussion key 6");

    juce::ValueTree tauLegacyRoot { "TauLegacyRoot" };
    auto tauLegacyLayout = LayoutWrapper::getLayoutTree(InstrumentType::Tau, tauLegacyRoot);
    auto tauLegacyPerc = tauLegacyLayout.getOrCreateChildWithName(LayoutWrapper::id_key + juce::String("_0_72"), nullptr);
    tauLegacyPerc.setProperty(LayoutWrapper::id_keyType, (int)EigenharpKeyType::Perc, nullptr);
    tauLegacyPerc.setProperty(LayoutWrapper::id_keyColour, (int)KeyColour::Off, nullptr);
    tauLegacyPerc.setProperty(LayoutWrapper::id_zone, (int)Zone::Zone3, nullptr);
    tauLegacyPerc.setProperty(LayoutWrapper::id_keyMappingType, (int)KeyMappingType::Note, nullptr);
    tauLegacyPerc.setProperty(LayoutWrapper::id_mappingValue, "49", nullptr);

    auto tauLegacyButton = tauLegacyLayout.getOrCreateChildWithName(LayoutWrapper::id_key + juce::String("_1_5"), nullptr);
    tauLegacyButton.setProperty(LayoutWrapper::id_keyType, (int)EigenharpKeyType::Button, nullptr);
    tauLegacyButton.setProperty(LayoutWrapper::id_keyColour, (int)KeyColour::Yellow, nullptr);
    tauLegacyButton.setProperty(LayoutWrapper::id_zone, (int)Zone::Zone1, nullptr);
    tauLegacyButton.setProperty(LayoutWrapper::id_keyMappingType, (int)KeyMappingType::None, nullptr);
    tauLegacyButton.setProperty(LayoutWrapper::id_mappingValue, {}, nullptr);

    auto migratedTauLegacyPerc = LayoutWrapper::getLayoutKey({ 1, 0, InstrumentType::Tau }, tauLegacyRoot);
    ok &= expect(migratedTauLegacyPerc.mappingValue == "49",
                 "legacy Tau percussion numbering should migrate to the current course 1 numbering");
    ok &= expect(tauLegacyLayout.getChildWithName(LayoutWrapper::id_key + juce::String("_1_0")).isValid(),
                 "legacy Tau percussion nodes should migrate to key_1_* entries");
    ok &= expect(!tauLegacyLayout.getChildWithName(LayoutWrapper::id_key + juce::String("_0_72")).isValid(),
                 "legacy Tau percussion nodes should be removed after migration");

    auto migratedTauLegacyButton = LayoutWrapper::getLayoutKey({ 2, 0, InstrumentType::Tau }, tauLegacyRoot);
    ok &= expect(migratedTauLegacyButton.keyType == EigenharpKeyType::Button,
                 "legacy Tau button numbering should migrate to the current course 2 numbering");
    ok &= expect(tauLegacyLayout.getChildWithName(LayoutWrapper::id_key + juce::String("_2_0")).isValid(),
                 "legacy Tau button nodes should migrate to key_2_* entries");
    ok &= expect(!tauLegacyLayout.getChildWithName(LayoutWrapper::id_key + juce::String("_1_5")).isValid(),
                 "legacy Tau button nodes should be removed after migration");

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