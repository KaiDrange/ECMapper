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
    for (auto sensitivity : {SettingsWrapper::id_rollSensitivity, SettingsWrapper::id_yawSensitivity, SettingsWrapper::id_pressureSensitivity})
        SettingsWrapper::setCalibrationValue(InstrumentType::Alpha, sensitivity, 1.0f, state.state);
    MidiService service(lookups, lock);
    service.start(state, nullptr);
    lookups[0].updateAll();
    // Inverted curves must not reverse joystick/Touche controls or move their centers.
    for (auto target : {ExpressionCurveTarget::Roll, ExpressionCurveTarget::Yaw, ExpressionCurveTarget::Pressure})
        lookups[0].expressionCurves[static_cast<int>(target)].setData({1.0f, {0.25f, 0.75f}, 0.5f, {0.75f, 0.25f}, 0.0f});
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
    LayoutWrapper::setKeyMappingType(id, KeyMappingType::Touche, state.state);
    settings.midiChannel = 7;
    settings.assignments = {{{MidiValueType::CC, 20, 0, 127}, {MidiValueType::CC, 21, 0, 127},
        {MidiValueType::CC, 22, 10, 110}, {MidiValueType::CC, 23, 20, 120},
        {MidiValueType::CC, 24, 0, 127}}}; // Hidden fifth slot must never emit.
    LayoutWrapper::setJoystickSettings(id, settings, state.state);
    copy = state.state.createCopy();
    if (LayoutWrapper::getJoystickSettings(id, copy).assignments[3].number != 23) return 17;
    LayoutWrapper::setKeyMappingType(id, KeyMappingType::Joystick, state.state);
    if (LayoutWrapper::getJoystickSettings(id, state.state).assignments[2].type != MidiValueType::Pitchbend) return 18;
    LayoutWrapper::setKeyMappingType(id, KeyMappingType::Touche, state.state);
    for (auto button : {LayoutWrapper::KeyId{1, 0, InstrumentType::Pico}, LayoutWrapper::KeyId{2, 0, InstrumentType::Tau}}) {
        LayoutWrapper::setKeyMappingType(button, KeyMappingType::Touche, state.state);
        if (LayoutWrapper::getLayoutKey(button, state.state).keyMappingType == KeyMappingType::Touche) return 19;
    }
    lookups[0].updateAll();
    // Inverted curves must not reverse joystick/Touche controls or move their centers.
    for (auto target : {ExpressionCurveTarget::Roll, ExpressionCurveTarget::Yaw, ExpressionCurveTarget::Pressure})
        lookups[0].expressionCurves[static_cast<int>(target)].setData({1.0f, {0.25f, 0.75f}, 0.5f, {0.75f, 0.25f}, 0.0f});
    if (key.mapType != KeyMappingType::Touche || key.output != MidiChannelType::Chan7) return 20;
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(lookups,
        service.getProtocol(), service.getVoiceRouter(), createExpressionEmissionPolicy(OutputTransportMode::Vst3Direct)));
    auto readPress = [&](float roll, float pressure, bool active) {
        sink.events.clear();
        msg.yaw = 1; msg.roll = roll; msg.pressure = pressure; msg.active = active;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        std::array<float, 2> values {-1, -1};
        for (const auto& event : sink.events) {
            if (event.controller == 22) values[0] = (event.value * 127.0f - 10.0f) / 100.0f;
            if (event.controller == 23) values[1] = (event.value * 127.0f - 20.0f) / 100.0f;
        }
        return values;
    };
    const auto center = readPress(0, 1, true);
    if (sink.events.size() != 4 || sink.events[0].controller != 20 || std::abs(sink.events[0].value) > 0.00001f
        || sink.events[1].controller != 21 || std::abs(sink.events[1].value - 1.0f) > 0.00001f) return 27;
    if (sink.events.size() != 4 || std::abs(center[0] - 1) > 0.00001f || std::abs(center[1] - 1) > 0.00001f) return 21;
    const auto negative = readPress(-1, 1, true);
    if (std::abs(negative[0] - 1) > 0.00001f || std::abs(negative[1]) > 0.00001f) return 22;
    const auto positive = readPress(1, 1, true);
    if (std::abs(positive[0]) > 0.00001f || std::abs(positive[1] - 1) > 0.00001f) return 23;
    const auto between = readPress(0.25f, 1, true);
    if (std::abs(between[0] - 0.75f) > 0.00001f || std::abs(between[1] - 1) > 0.00001f) return 24;
    const auto soft = readPress(0, 0.25f, true);
    if (std::abs(soft[0] - 0.25f) > 0.00001f || std::abs(soft[0] - soft[1]) > 0.00001f) return 25;
    const auto released = readPress(1, 1, false);
    if (sink.events.size() != 4 || std::abs(released[0]) > 0.00001f || std::abs(released[1]) > 0.00001f) return 26;
    // Negative yaw is a distance from center, not an inverted CC range.
    auto readYaw = [&](float yaw, bool active) {
        sink.events.clear();
        msg.yaw = yaw; msg.roll = 0; msg.pressure = 0; msg.active = active;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        for (const auto& event : sink.events)
            if (event.controller == 20) return event.value;
        return -1.0f;
    };
    const auto yawCenter = readYaw(0, true);
    const auto yawHalf = readYaw(-0.25f, true);
    const auto yawFull = readYaw(-1, true);
    const auto yawRelease = readYaw(-1, false);
    if (std::abs(yawCenter) > 0.00001f || std::abs(yawHalf - 0.25f) > 0.00001f
        || std::abs(yawFull - 1) > 0.00001f || std::abs(yawRelease) > 0.00001f) return 28;

    // Physical percussion keys use roll directions and yaw pressure positioning.
    const LayoutWrapper::KeyId percussionId {1, 0, InstrumentType::Alpha};
    LayoutWrapper::setKeyMappingType(percussionId, KeyMappingType::Touche, state.state);
    LayoutWrapper::setJoystickSettings(percussionId, settings, state.state);
    lookups[0].updateKey(percussionId);
    if (lookups[0].keys[1][0].keyType != EigenharpKeyType::Perc) return 29;
    service.setRuntimeConfigSnapshot(std::make_unique<MidiService::RuntimeConfigSnapshot>(lookups,
        service.getProtocol(), service.getVoiceRouter(), createExpressionEmissionPolicy(OutputTransportMode::Vst3Direct)));
    msg.course = 1;
    auto readPercussion = [&](float roll, float yaw, bool active) {
        sink.events.clear();
        msg.roll = roll; msg.yaw = yaw; msg.pressure = 1; msg.active = active;
        service.processMessage(msg, outgoing, buffer, sink, 0, nullptr);
        std::array<float, 4> values {-1, -1, -1, -1};
        for (const auto& event : sink.events) {
            if (event.controller == 20) values[0] = event.value;
            if (event.controller == 21) values[1] = event.value;
            if (event.controller == 22) values[2] = (event.value * 127.0f - 10.0f) / 100.0f;
            if (event.controller == 23) values[3] = (event.value * 127.0f - 20.0f) / 100.0f;
        }
        return values;
    };
    const auto percCenter = readPercussion(-1, 0, true);
    if (std::abs(percCenter[0] - 1) > 0.00001f || std::abs(percCenter[1]) > 0.00001f
        || std::abs(percCenter[2] - 1) > 0.00001f || std::abs(percCenter[3] - 1) > 0.00001f) return 30;
    const auto percNegative = readPercussion(1, -1, true);
    if (std::abs(percNegative[0]) > 0.00001f || std::abs(percNegative[1] - 1) > 0.00001f
        || std::abs(percNegative[2] - 1) > 0.00001f || std::abs(percNegative[3]) > 0.00001f) return 31;
    const auto percBetween = readPercussion(0, 0.25f, true);
    if (std::abs(percBetween[2] - 0.75f) > 0.00001f || std::abs(percBetween[3] - 1) > 0.00001f) return 32;
    const auto percPositive = readPercussion(0, 1, true);
    if (std::abs(percPositive[2]) > 0.00001f || std::abs(percPositive[3] - 1) > 0.00001f) return 33;
    const auto percRelease = readPercussion(1, 1, false);
    for (float value : percRelease)
        if (std::abs(value) > 0.00001f) return 34;
    std::cout << "JoystickTest passed\n";
    return 0;
}
