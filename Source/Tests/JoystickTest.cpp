#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#include <JuceHeader.h>
#include "Core/MidiService.h"
#include "Core/SettingsWrapper.h"
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

static ecm::osc::Message makeKeyMessage(float pressure, bool active, uint64_t timestamp) {
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


int main() {
    juce::ScopedJuceInitialiser_GUI init;
    using namespace ecm;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "TestState", {});
    juce::CriticalSection lock;
    const LayoutWrapper::KeyId id {0, 0, InstrumentType::Alpha};
    LayoutWrapper::setKeyMappingType(id, KeyMappingType::Joystick, state.state);
    LayoutWrapper::JoystickSettings settings;
    settings.midiChannel = 7;
    settings.assignments = {{{MidiValueType::CC, 20, 12, 112}, {MidiValueType::CC, 21, 5, 125},
        {MidiValueType::Pitchbend, 0, 8192, 0}, {MidiValueType::CC, 22, 10, 100},
        {MidiValueType::CC, 23, 15, 127}}};
    LayoutWrapper::setJoystickSettings(id, settings, state.state);
    auto copy = state.state.createCopy();
    if (LayoutWrapper::getJoystickSettings(id, copy).assignments[2].minimum != 8192) return 1;
    for (auto button : {LayoutWrapper::KeyId{1, 0, InstrumentType::Pico}, LayoutWrapper::KeyId{2, 0, InstrumentType::Tau}}) {
        LayoutWrapper::setKeyMappingType(button, KeyMappingType::Joystick, state.state);
        if (LayoutWrapper::getLayoutKey(button, state.state).keyMappingType == KeyMappingType::Joystick) return 2;
    }
    if (!LayoutWrapper::supportsJoystick({1, 0, InstrumentType::Tau})) return 3;
    ConfigLookup lookups[] = {ConfigLookup(InstrumentType::Alpha, state, lock),
        ConfigLookup(InstrumentType::Tau, state, lock), ConfigLookup(InstrumentType::Pico, state, lock)};
    MidiService service(lookups, lock);
    service.start(state, nullptr);
    lookups[0].updateAll();
    if (lookups[0].keys[0][0].output != MidiChannelType::Chan7) return 4;
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(lookups,
        service.getProtocol(), service.getVoiceRouter(), createExpressionEmissionPolicy(OutputTransportMode::Vst3Direct)));
    CapturingSink sink;
    osc::Message outgoing;
    juce::MidiBuffer buffer;
    auto msg = makeKeyMessage(1.0f, true, 1000000);
    msg.roll = -1.0f;
    msg.yaw = -1.0f;
    service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
    if (sink.events.size() != 5 || sink.events[0].value <= 12.0f / 127.0f
        || std::abs(sink.events[1].value - 5.0f / 127.0f) > 0.00001f
        || sink.events[2].value >= 8192.0f / 16383.0f) return 5;
    sink.events.clear();
    msg.roll = 1.0f; msg.yaw = 1.0f;
    service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
    if (sink.events.size() != 5 || std::abs(sink.events[0].value - 12.0f / 127.0f) > 0.00001f
        || sink.events[1].value <= 5.0f / 127.0f || sink.events[3].value <= 10.0f / 127.0f) return 6;
    sink.events.clear();
    msg.active = false; // Nonzero sensor values on release must be ignored.
    service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
    if (sink.events.size() != 5) return 7;
    for (size_t i = 0; i < 5; ++i) {
        const float limit = i == 2 ? 16383.0f : 127.0f;
        if (sink.events[i].channel != 7 || std::abs(sink.events[i].value - static_cast<float>(settings.assignments[i].minimum) / limit) > 0.00001f) return 8;
    }
    if (sink.events[4].controller != 23) return 9;
    sink.events.clear();
    settings.midiChannel = 0;
    settings.assignments[0] = {MidiValueType::Pitchbend, 0, 8192, 0};
    settings.assignments[1] = {MidiValueType::Pitchbend, 0, 8192, 16383};
    settings.assignments[2].type = MidiValueType::Off;
    settings.assignments[3].type = MidiValueType::Off;
    settings.assignments[4].type = MidiValueType::Off;
    auto& key = lookups[0].keys[0][0];
    key.joystick = settings;
    key.output = MidiChannelType::MPE_High;
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(lookups,
        service.getProtocol(), service.getVoiceRouter(), createExpressionEmissionPolicy(OutputTransportMode::Vst3Direct)));
    msg.active = true; msg.roll = -1.0f;
    service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
    if (sink.events.size() != 1 || sink.events[0].channel != 16 || sink.events[0].value >= 0.5f) return 10;
    sink.events.clear();
    msg.roll = 1.0f;
    service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
    if (sink.events.size() != 1 || sink.events[0].value <= 0.5f) return 11;
    // MIDI 1 and MIDI 2 share note-expression cadence; release must never wait for it.
    for (auto mode : {OutputTransportMode::LegacyMidi, OutputTransportMode::UmpMidi}) {
        msg.active = false;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        sink.events.clear();
        auto policy = createExpressionEmissionPolicy(mode);
        const int stride = policy->getConfig().minMessageStride;
        service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(lookups,
            service.getProtocol(), service.getVoiceRouter(), policy));
        msg.active = true;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        if (sink.events.size() != 1) return 12;
        sink.events.clear();
        for (int i = 1; i < stride; ++i)
            service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        if (!sink.events.empty()) return 13;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        if (sink.events.size() != 1) return 14;
        sink.events.clear();
        msg.active = false;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        if (sink.events.size() != 2) return 15;
        for (const auto& event : sink.events)
            if (std::abs(event.value - 8192.0f / 16383.0f) > 0.00001f) return 16;
    }
    std::cout << "JoystickTest passed\n";
    return 0;
}
