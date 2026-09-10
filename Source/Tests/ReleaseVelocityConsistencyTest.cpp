#include <cmath>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#include "Core/MidiService.h"
#include "Core/OSCMessage.h"
#include "Core/PerformanceEventSink.h"
#include "Core/SettingsWrapper.h"

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

    const juce::String getName() const override { return "ReleaseVelocityConsistencyTest"; }
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

class CapturingSink final : public ecm::PerformanceEventSink {
public:
    void pushEvent(const ecm::PerformanceEvent& event) override {
        events.push_back(event);
    }

    std::vector<ecm::PerformanceEvent> events;
};

ecm::osc::Message makeKeyMessage(float pressure, bool active, uint64_t timestamp) {
    ecm::osc::Message message;
    message.type = ecm::osc::MessageType::Key;
    message.device = ecm::InstrumentType::Alpha;
    message.course = 0;
    message.key = 0;
    message.active = active;
    message.pressure = pressure;
    message.roll = 0.0f;
    message.yaw = 0.0f;
    message.timestamp = timestamp;
    std::strncpy(message.devId, "test-device", 63);
    return message;
}

void configureMappedKey(ecm::ConfigLookup& configLookup) {
    auto& key = configLookup.keys[0][0];
    key.keyId = { 0, 0, ecm::InstrumentType::Alpha };
    key.mapType = ecm::KeyMappingType::Note;
    key.notes = { 60, -1, -1, -1 };
    key.output = ecm::MidiChannelType::Chan1;
    key.pressure.valueType = ecm::MidiValueType::Off;
    key.roll.valueType = ecm::MidiValueType::Off;
    key.yaw.valueType = ecm::MidiValueType::Off;
}

const ecm::PerformanceEvent* findNoteOff(const std::vector<ecm::PerformanceEvent>& events) {
    for (const auto& event : events) {
        if (event.kind == ecm::PerformanceEventKind::NoteOff)
            return &event;
    }

    return nullptr;
}

std::optional<float> captureReleaseVelocity(const std::vector<float>& activePressures, float releasePressure) {
    using namespace ecm;

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    SettingsWrapper::setMidi2Mode(false, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    configureMappedKey(configLookups[0]);

    midiService.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(
        configLookups,
        midiService.getProtocol(),
        midiService.getVoiceRouter(),
        midiService.getExpressionPolicy()));

    CapturingSink sink;
    osc::Message outgoingMessage;
    juce::MidiBuffer midiBuffer;
    const uint64_t startTimestamp = 1'000'000ULL;

    for (size_t i = 0; i < activePressures.size(); ++i) {
        auto message = makeKeyMessage(activePressures[i], true, startTimestamp + static_cast<uint64_t>(i) * 20'000ULL);
        midiService.processMessage(message, outgoingMessage, midiBuffer, sink, 0, nullptr);
    }

    auto releaseMessage = makeKeyMessage(releasePressure, false, startTimestamp + static_cast<uint64_t>(activePressures.size()) * 20'000ULL);
    midiService.processMessage(releaseMessage, outgoingMessage, midiBuffer, sink, 0, nullptr);

    const auto* noteOff = findNoteOff(sink.events);
    midiService.stop();

    if (noteOff == nullptr)
        return std::nullopt;

    return noteOff->velocity;
}

bool verifyStaleSampleDoesNotDominateReleaseVelocity() {
    const auto lowOldest = captureReleaseVelocity({ 0.02f, 0.04f, 0.06f, 0.02f, 0.09f, 0.08f, 0.07f, 0.06f }, 0.01f);
    const auto highOldest = captureReleaseVelocity({ 0.02f, 0.04f, 0.06f, 0.08f, 0.09f, 0.08f, 0.07f, 0.06f }, 0.01f);

    bool ok = true;
    ok &= expect(lowOldest.has_value(), "expected note-off velocity for the low-oldest sequence");
    ok &= expect(highOldest.has_value(), "expected note-off velocity for the high-oldest sequence");

    if (lowOldest.has_value() && highOldest.has_value()) {
        ok &= expect(std::abs(*lowOldest - *highOldest) < 0.10f,
                     "release velocity should stay similar when only a stale oldest sample changes");
    }

    return ok;
}

bool verifyRecentReleaseShapeDrivesReleaseVelocity() {
    const auto slowRelease = captureReleaseVelocity({ 0.02f, 0.03f, 0.04f, 0.05f, 0.07f, 0.07f, 0.06f, 0.05f }, 0.04f);
    const auto quickRelease = captureReleaseVelocity({ 0.02f, 0.03f, 0.04f, 0.05f, 0.07f, 0.07f, 0.06f, 0.05f }, 0.01f);

    bool ok = true;
    ok &= expect(slowRelease.has_value(), "expected note-off velocity for the slow release");
    ok &= expect(quickRelease.has_value(), "expected note-off velocity for the quick release");

    if (slowRelease.has_value() && quickRelease.has_value()) {
        ok &= expect(*quickRelease > *slowRelease + 0.15f,
                     "release velocity should rise when the final pressure drop is quicker");
    }

    return ok;
}

bool verifyFastReleaseDoesNotStickToBottomOfRange() {
    const auto fastRelease = captureReleaseVelocity({ 0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.04f }, 0.005f);

    bool ok = true;
    ok &= expect(fastRelease.has_value(), "expected note-off velocity for the fast release");

    if (fastRelease.has_value()) {
        ok &= expect(*fastRelease > 0.40f,
                     "fast releases should not stay near the bottom of the release-velocity range");
    }

    return ok;
}

bool verifyQuickReleaseIsNotDrivenByAbsolutePressure() {
    const auto softQuickRelease = captureReleaseVelocity({ 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.05f, 0.04f, 0.03f }, 0.01f);
    const auto hardQuickRelease = captureReleaseVelocity({ 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.50f, 0.40f, 0.30f }, 0.10f);

    bool ok = true;
    ok &= expect(softQuickRelease.has_value(), "expected note-off velocity for the soft quick release");
    ok &= expect(hardQuickRelease.has_value(), "expected note-off velocity for the hard quick release");

    if (softQuickRelease.has_value() && hardQuickRelease.has_value()) {
        ok &= expect(std::abs(*softQuickRelease - *hardQuickRelease) < 0.12f,
                     "equally quick releases should stay similar even when the absolute held pressure changes");
    }

    return ok;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    bool ok = true;
    ok &= verifyStaleSampleDoesNotDominateReleaseVelocity();
    ok &= verifyRecentReleaseShapeDrivesReleaseVelocity();
    ok &= verifyFastReleaseDoesNotStickToBottomOfRange();
    ok &= verifyQuickReleaseIsNotDrivenByAbsolutePressure();

    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "ReleaseVelocityConsistencyTest passed" << std::endl;
    return 0;
}