#include <iostream>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/LayoutChangeHandler.h"
#include "Core/LayoutWrapper.h"
#include "Core/OSCMessage.h"
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

    pluginState.state.removeListener(&handler);
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "LayoutLoadListenerTest passed" << std::endl;
    return 0;
}