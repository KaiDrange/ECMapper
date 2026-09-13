#include <iostream>

#include <JuceHeader.h>

#include "Core/ConfigLookup.h"
#define private public
#include "Core/MidiService.h"
#undef private
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

    const juce::String getName() const override { return "MPEZoneLayoutConfigurationTest"; }
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

juce::var decodePropertyReplyBody(const juce::midi_ci::PropertyReplyData& reply)
{
    return juce::midi_ci::Encodings::jsonFrom7BitText(reply.body);
}

bool expectIntProperty(const juce::var& object, const juce::Identifier& property, int expectedValue, const char* message)
{
    if (const auto* dynamicObject = object.getDynamicObject())
        return expect(static_cast<int>(dynamicObject->getProperty(property)) == expectedValue, message);

    return expect(false, message);
}

bool expectBoolProperty(const juce::var& object, const juce::Identifier& property, bool expectedValue, const char* message)
{
    if (const auto* dynamicObject = object.getDynamicObject())
        return expect(static_cast<bool>(dynamicObject->getProperty(property)) == expectedValue, message);

    return expect(false, message);
}

const juce::DynamicObject* findResourceListEntry(const juce::var& resourceList, const juce::String& resourceName)
{
    if (const auto* entries = resourceList.getArray()) {
        for (const auto& entry : *entries) {
            if (const auto* entryObject = entry.getDynamicObject()) {
                if (entryObject->getProperty("resource").toString() == resourceName)
                    return entryObject;
            }
        }
    }

    return nullptr;
}

juce::MPEZoneLayout collectStartupLayout(ecm::MidiService& midiService) {
    juce::MidiBuffer setupBuffer;
    midiService.drainPendingMidiMessages(setupBuffer);

    juce::MPEZoneLayout layout;
    layout.processNextMidiBuffer(setupBuffer);
    return layout;
}

bool verifyStartupLayoutUsesConfiguredPerNoteRangesAndDefaultMasterRange() {
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
    SettingsWrapper::setLowerMPEVoiceCount(9, pluginState.state);
    SettingsWrapper::setUpperMPEVoiceCount(4, pluginState.state);
    SettingsWrapper::setLowerMPEPB(17, pluginState.state);
    SettingsWrapper::setUpperMPEPB(29, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    const auto layout = collectStartupLayout(midiService);
    midiService.stop();

    bool ok = true;
    ok &= expect(layout.getLowerZone().numMemberChannels == 9,
                 "lower zone should use the configured member channel count");
    ok &= expect(layout.getLowerZone().perNotePitchbendRange == 17,
                 "lower zone per-note pitch bend should come from settings");
    ok &= expect(layout.getLowerZone().masterPitchbendRange == 12,
                 "lower zone master pitch bend should use the ECMapper default of 12 semitones");
    ok &= expect(layout.getUpperZone().numMemberChannels == 4,
                 "upper zone should use the configured member channel count when lower zone leaves room");
    ok &= expect(layout.getUpperZone().perNotePitchbendRange == 29,
                 "upper zone per-note pitch bend should come from settings");
    ok &= expect(layout.getUpperZone().masterPitchbendRange == 12,
                 "upper zone master pitch bend should use the ECMapper default of 12 semitones");
    return ok;
}

bool verifyFullLowerZoneDisablesUpperZoneButKeepsConfiguredPerNoteRange() {
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
    SettingsWrapper::setLowerMPEVoiceCount(14, pluginState.state);
    SettingsWrapper::setUpperMPEVoiceCount(4, pluginState.state);
    SettingsWrapper::setLowerMPEPB(36, pluginState.state);
    SettingsWrapper::setUpperMPEPB(29, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);
    const auto layout = collectStartupLayout(midiService);
    midiService.stop();

    bool ok = true;
    ok &= expect(layout.getLowerZone().numMemberChannels == 14,
                 "lower zone should allow the configured full member channel count");
    ok &= expect(layout.getLowerZone().perNotePitchbendRange == 36,
                 "full lower zone should still use the configured per-note pitch bend range");
    ok &= expect(layout.getLowerZone().masterPitchbendRange == 12,
                 "full lower zone should still keep the ECMapper default master pitch bend range");
    ok &= expect(layout.getUpperZone().numMemberChannels == 0,
                 "upper zone should stay inactive when the lower zone consumes all member channels");
    return ok;
}

bool verifyCIPropertyReportsCurrentMPELayout() {
    using namespace ecm;
    const auto requestMuid = juce::midi_ci::MUID::makeUnchecked(0);

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    SettingsWrapper::setMidi2Mode(true, pluginState.state);
    SettingsWrapper::setLowerMPEVoiceCount(8, pluginState.state);
    SettingsWrapper::setUpperMPEVoiceCount(3, pluginState.state);
    SettingsWrapper::setLowerMPEPB(19, pluginState.state);
    SettingsWrapper::setUpperMPEPB(31, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);

    juce::midi_ci::PropertyRequestHeader resourceListRequest;
    resourceListRequest.resource = "ResourceList";
    const auto resourceListReply = midiService.propertyGetDataRequested(requestMuid, resourceListRequest);

    juce::midi_ci::PropertyRequestHeader layoutRequest;
    layoutRequest.resource = "X-ECMapperMPELayout";
    const auto layoutReply = midiService.propertyGetDataRequested(requestMuid, layoutRequest);

    juce::midi_ci::PropertyRequestHeader deviceInfoRequest;
    deviceInfoRequest.resource = "DeviceInfo";
    const auto deviceInfoReply = midiService.propertyGetDataRequested(requestMuid, deviceInfoRequest);

    juce::midi_ci::PropertyRequestHeader unknownRequest;
    unknownRequest.resource = "DoesNotExist";
    const auto unknownReply = midiService.propertyGetDataRequested(requestMuid, unknownRequest);

    midiService.stop();

    bool ok = true;
    ok &= expect(resourceListReply.header.status == 200,
                 "resource list request should succeed");
    const auto resourceList = decodePropertyReplyBody(resourceListReply);
    ok &= expect(resourceList.isArray() && resourceList.getArray()->size() >= 2,
                 "resource list should advertise both standard and custom properties");

    if (const auto* deviceInfoEntry = findResourceListEntry(resourceList, "DeviceInfo")) {
        ok &= expect(! deviceInfoEntry->hasProperty("canGet"),
                     "resource list should omit canGet for the standard DeviceInfo property");
    } else {
        ok &= expect(false, "resource list should advertise the standard DeviceInfo property");
    }

    if (const auto* layoutEntry = findResourceListEntry(resourceList, "X-ECMapperMPELayout")) {
        ok &= expect(static_cast<bool>(layoutEntry->getProperty("canGet")),
                     "resource list should mark the MPE layout property as readable");
        ok &= expect(layoutEntry->hasProperty("schema"),
                     "resource list should provide schema metadata for the custom MPE layout property");
    } else {
        ok &= expect(false, "resource list should advertise the MPE layout property");
    }

    ok &= expect(deviceInfoReply.header.status == 200,
                 "DeviceInfo property request should succeed");
    const auto deviceInfoData = decodePropertyReplyBody(deviceInfoReply);
    const auto* deviceInfoObject = deviceInfoData.getDynamicObject();
    ok &= expect(deviceInfoObject != nullptr,
                 "DeviceInfo should return a JSON object");

    if (deviceInfoObject != nullptr) {
        ok &= expect(deviceInfoObject->getProperty("manufacturer").toString() == "ECMapper",
                     "DeviceInfo should report the ECMapper manufacturer name");
        ok &= expect(deviceInfoObject->getProperty("model").toString().containsIgnoreCase("ECMapper"),
                     "DeviceInfo should report an ECMapper model name");
        ok &= expect(deviceInfoObject->getProperty("version").toString() == ProjectInfo::versionString,
                     "DeviceInfo should report the current ECMapper version");
    }

    ok &= expect(layoutReply.header.status == 200,
                 "MPE layout property request should succeed");
    const auto layoutData = decodePropertyReplyBody(layoutReply);
    const auto* layoutObject = layoutData.getDynamicObject();
    ok &= expect(layoutObject != nullptr,
                 "MPE layout property should return a JSON object");

    if (layoutObject != nullptr) {
        ok &= expect(layoutObject->getProperty("resource").toString() == "X-ECMapperMPELayout",
                     "MPE layout property should identify its resource name");

        const auto lowerZone = layoutObject->getProperty("lowerZone");
        const auto upperZone = layoutObject->getProperty("upperZone");

        ok &= expectIntProperty(lowerZone, "memberChannels", 8,
                                "CI lower-zone member channel count should match settings");
        ok &= expectIntProperty(lowerZone, "perNotePitchbendRange", 19,
                                "CI lower-zone per-note pitch bend should match settings");
        ok &= expectIntProperty(lowerZone, "masterPitchbendRange", 12,
                                "CI lower-zone master pitch bend should match the ECMapper default");
        ok &= expectBoolProperty(lowerZone, "active", true,
                                 "CI lower zone should report itself as active");

        ok &= expectIntProperty(upperZone, "memberChannels", 3,
                                "CI upper-zone member channel count should match settings");
        ok &= expectIntProperty(upperZone, "perNotePitchbendRange", 31,
                                "CI upper-zone per-note pitch bend should match settings");
        ok &= expectIntProperty(upperZone, "masterPitchbendRange", 12,
                                "CI upper-zone master pitch bend should match the ECMapper default");
        ok &= expectBoolProperty(upperZone, "active", true,
                                 "CI upper zone should report itself as active when configured");
    }

    ok &= expect(unknownReply.header.status == 404,
                 "unknown property requests should be rejected");
    return ok;
}

bool verifyMidi2DoesNotAdvertiseLocalMPEProfile()
{
    using namespace ecm;
    using namespace juce::midi_ci;

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState pluginState(processor, nullptr, "TestState", {});
    juce::CriticalSection stateLock;
    ConfigLookup configLookups[] = {
        ConfigLookup(InstrumentType::Alpha, pluginState, stateLock),
        ConfigLookup(InstrumentType::Tau, pluginState, stateLock),
        ConfigLookup(InstrumentType::Pico, pluginState, stateLock)
    };

    SettingsWrapper::setMidi2Mode(true, pluginState.state);

    MidiService midiService(configLookups, stateLock);
    midiService.start(pluginState, nullptr);

    bool ok = true;
    ok &= expect(juce::MessageManager::getInstance()->runDispatchLoopUntil(50),
                 "MIDI-CI startup should complete its async initialization");

    const auto* host = midiService.ciDevice_ != nullptr ? midiService.ciDevice_->getProfileHost() : nullptr;
    ok &= expect(host != nullptr,
                 "MIDI-CI profile host should exist in MIDI 2 mode");

    if (host != nullptr)
    {
        const auto* groupState = host->getProfileStates().getStateForDestination(
            ChannelAddress().withGroup(0).withChannel(ChannelInGroup::wholeGroup));
        const auto* channelState = host->getProfileStates().getStateForDestination(
            ChannelAddress().withGroup(0).withChannel(ChannelInGroup::channel0));

        ok &= expect(groupState != nullptr,
                     "group profile state should be inspectable");
        ok &= expect(channelState != nullptr,
                     "channel profile state should be inspectable");

        if (groupState != nullptr)
            ok &= expect(groupState->empty(),
                         "ECMapper should not advertise a group-scoped local MPE profile in MIDI 2 mode");

        if (channelState != nullptr)
            ok &= expect(channelState->empty(),
                         "ECMapper should not advertise a channel-scoped local MPE profile in MIDI 2 mode");
    }

    midiService.stop();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    return ok;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    SilentLogger logger;
    juce::Logger::setCurrentLogger(&logger);

    const bool ok = verifyStartupLayoutUsesConfiguredPerNoteRangesAndDefaultMasterRange()
                 && verifyFullLowerZoneDisablesUpperZoneButKeepsConfiguredPerNoteRange()
                 && verifyCIPropertyReportsCurrentMPELayout()
                 && verifyMidi2DoesNotAdvertiseLocalMPEProfile();
    juce::Logger::setCurrentLogger(nullptr);

    if (!ok)
        return 1;

    std::cout << "MPEZoneLayoutConfigurationTest passed" << std::endl;
    return 0;
}