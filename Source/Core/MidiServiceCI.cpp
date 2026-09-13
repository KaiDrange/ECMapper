#include "MidiService.h"

namespace ecm {

namespace {

constexpr auto deviceInfoPropertyResource = "DeviceInfo";
constexpr auto mpeLayoutPropertyResource = "X-ECMapperMPELayout";

template <size_t Size>
juce::var createByteArrayProperty(const std::array<std::byte, Size>& bytes)
{
    juce::Array<juce::var> values;
    for (auto byte : bytes)
        values.add(static_cast<int>(byte));

    return values;
}

// MIDI-CI profile identifiers are 5-byte values defined by the MIDI-CI Profile
// Configuration message set. For MPE, ECMapper only needs to recognise the
// standard profile ID regardless of whether the profile is reported for the
// lower zone (`0x7E`) or upper zone (`0x7F`). The byte layout checked here is:
//   [0] profile / bank selector: `0x7E` lower-zone MPE or `0x7F` upper-zone MPE
//   [1] profile number MSB: `0x01`
//   [2] profile number LSB: ignored here because JUCE already normalises the
//       profile object and ECMapper only distinguishes MPE vs non-MPE
//   [3] profile version / level byte: `0x01` for the MPE profile definition
//   [4] profile level / optional data: not used here
// For the normative layout of Profile identifiers, see the MIDI-CI/Profile
// specification sections describing Standard Profiles and MPE profile IDs.
bool isMPEProfile(const juce::midi_ci::Profile& profile)
{
    return (profile[0] == std::byte { 0x7E } || profile[0] == std::byte { 0x7F })
        && profile[1] == std::byte { 0x01 }
        && profile[3] == std::byte { 0x01 };
}

juce::var createMPELayoutPropertySchema()
{
    const auto createIntegerSchema = []() {
        auto schema = std::make_unique<juce::DynamicObject>();
        schema->setProperty("type", "integer");
        return juce::var(schema.release());
    };

    const auto createBooleanSchema = []() {
        auto schema = std::make_unique<juce::DynamicObject>();
        schema->setProperty("type", "boolean");
        return juce::var(schema.release());
    };

    auto zoneProperties = std::make_unique<juce::DynamicObject>();
    zoneProperties->setProperty("memberChannels", createIntegerSchema());
    zoneProperties->setProperty("perNotePitchbendRange", createIntegerSchema());
    zoneProperties->setProperty("masterPitchbendRange", createIntegerSchema());
    zoneProperties->setProperty("active", createBooleanSchema());

    auto zoneSchema = std::make_unique<juce::DynamicObject>();
    zoneSchema->setProperty("type", "object");
    zoneSchema->setProperty("properties", juce::var(zoneProperties.release()));
    zoneSchema->setProperty("required", juce::Array<juce::var> {
        "memberChannels", "perNotePitchbendRange", "masterPitchbendRange", "active"
    });

    auto rootProperties = std::make_unique<juce::DynamicObject>();
    rootProperties->setProperty("resource", [] {
        auto schema = std::make_unique<juce::DynamicObject>();
        schema->setProperty("const", mpeLayoutPropertyResource);
        return juce::var(schema.release());
    }());
    rootProperties->setProperty("lowerZone", juce::var(zoneSchema->clone().release()));
    rootProperties->setProperty("upperZone", juce::var(zoneSchema.release()));

    auto schema = std::make_unique<juce::DynamicObject>();
    schema->setProperty("type", "object");
    schema->setProperty("properties", juce::var(rootProperties.release()));
    schema->setProperty("required", juce::Array<juce::var> { "resource", "lowerZone", "upperZone" });
    return schema.release();
}

juce::var createDeviceInfoPropertyResourceListEntry()
{
    auto resource = std::make_unique<juce::DynamicObject>();
    resource->setProperty("resource", deviceInfoPropertyResource);
    // resource->setProperty("canGet", true);
    resource->setProperty("mediaTypes", juce::Array<juce::var> { "application/json" });
    resource->setProperty("encodings", juce::Array<juce::var> { "ASCII" });
    return resource.release();
}

juce::var createMPELayoutPropertyResourceListEntry()
{
    auto resource = std::make_unique<juce::DynamicObject>();
    resource->setProperty("resource", mpeLayoutPropertyResource);
    resource->setProperty("canGet", true);
    resource->setProperty("mediaTypes", juce::Array<juce::var> { "application/json" });
    resource->setProperty("encodings", juce::Array<juce::var> { "ASCII" });
    resource->setProperty("schema", createMPELayoutPropertySchema());
    return resource.release();
}

juce::var createZoneLayoutProperty(const juce::MPEZoneLayout::Zone& zone)
{
    auto zoneInfo = std::make_unique<juce::DynamicObject>();
    zoneInfo->setProperty("memberChannels", zone.numMemberChannels);
    zoneInfo->setProperty("perNotePitchbendRange", zone.perNotePitchbendRange);
    zoneInfo->setProperty("masterPitchbendRange", zone.masterPitchbendRange);
    zoneInfo->setProperty("active", zone.isActive());
    return zoneInfo.release();
}

juce::midi_ci::PropertyReplyData createMPELayoutPropertyReply(const juce::MPEZoneLayout& layout)
{
    auto body = std::make_unique<juce::DynamicObject>();
    body->setProperty("resource", mpeLayoutPropertyResource);
    body->setProperty("lowerZone", createZoneLayoutProperty(layout.getLowerZone()));
    body->setProperty("upperZone", createZoneLayoutProperty(layout.getUpperZone()));

    return { {}, juce::midi_ci::Encodings::jsonTo7BitText(body.release()) };
}

juce::midi_ci::PropertyReplyData createDeviceInfoPropertyReply()
{
    auto body = std::make_unique<juce::DynamicObject>();
    // These numeric arrays mirror the common Device Identity style fields used by
    // MIDI-CI `DeviceInfo` JSON resources:
    //   manufacturerId = 3-byte SysEx manufacturer ID (`0x7D` = educational / development ID)
    //   familyId       = 2-byte manufacturer-defined product family
    //   modelId        = 2-byte manufacturer-defined model identifier
    //   versionId      = 4-byte manufacturer-defined version/revision identifier
    // The exact JSON property names come from the MIDI-CI Property Exchange
    // `DeviceInfo` resource conventions.
    body->setProperty("manufacturerId", createByteArrayProperty(std::array { std::byte { 0x7D }, std::byte { 0x00 }, std::byte { 0x00 } }));
    body->setProperty("manufacturer", "ECMapper");
    body->setProperty("familyId", createByteArrayProperty(std::array { std::byte { 0x01 }, std::byte { 0x00 } }));
    body->setProperty("family", "ECMapper");
    body->setProperty("modelId", createByteArrayProperty(std::array { std::byte { 0x01 }, std::byte { 0x00 } }));
    body->setProperty("model", "ECMapper MIDI Service");
    body->setProperty("versionId", createByteArrayProperty(std::array { std::byte { 0x01 }, std::byte { 0x00 }, std::byte { 0x00 }, std::byte { 0x00 } }));
    body->setProperty("version", ProjectInfo::versionString);

    return { {}, juce::midi_ci::Encodings::jsonTo7BitText(body.release()) };
}

}

void MidiService::profileEnablementRequested (juce::midi_ci::MUID, juce::midi_ci::ProfileAtAddress profileAtAddress, int numChannels, bool enabled)
{
    if (!initialized_ || !ciDevice_)
        return;

    if (auto* host = ciDevice_->getProfileHost())
        host->setProfileEnablement(profileAtAddress, enabled ? numChannels : 0);
}

void MidiService::deviceAdded (juce::midi_ci::MUID x)
{
    if (!initialized_ || !ciDevice_)
        return;

    if (x == ciDevice_->getMuid())
        return;

    juce::Logger::writeToLog("MidiService: Remote MIDI-CI device discovered: 0x" + juce::String::toHexString(x.get()));

    auto disc = juce::universal_midi_packets::Factory::makeEndpointDiscovery(1, 1, std::byte { 0x0f });
    const auto* data = reinterpret_cast<const uint32_t*>(disc.data());
    const auto numWords = static_cast<size_t>(disc.size());

    const juce::ScopedLock sl(umpOutputLock_);
    if (isVirtualTarget_ && isMidi2Mode_)
    {
        for (auto& directUmpOutput : directUmpOutputs_)
        {
            if (!directUmpOutput.isAlive())
                continue;

            juce::universal_midi_packets::Iterator begin(data, numWords);
            juce::universal_midi_packets::Iterator end(data + numWords, 0);
            directUmpOutput.send(begin, end);
        }
    }
    else if (umpOutput_.isAlive())
    {
        juce::universal_midi_packets::Iterator begin(data, numWords);
        juce::universal_midi_packets::Iterator end(data + numWords, 0);
        umpOutput_.send(begin, end);
    }

    ciDevice_->sendProfileInquiry(x, juce::midi_ci::ChannelInGroup::wholeGroup);
    ciDevice_->sendProfileInquiry(x, juce::midi_ci::ChannelInGroup::channel0);
    ciDevice_->sendPropertyCapabilitiesInquiry(x);

    if (!isMidi2Mode_)
    {
        juce::Logger::writeToLog("MidiService: Proactively initiating Protocol Negotiation for MUID 0x" + juce::String::toHexString(x.get()));
        sendInitiateProtocolNegotiation(0, x);
    }
    else
    {
        juce::Logger::writeToLog("MidiService: Skipping proactively Protocol Negotiation for MUID 0x" + juce::String::toHexString(x.get()) + " (Already in MIDI 2.0 mode)");
    }
}

void MidiService::deviceRemoved (juce::midi_ci::MUID x)
{
    juce::Logger::writeToLog("MidiService: Remote MIDI-CI device removed: 0x" + juce::String::toHexString(x.get()));
}

juce::midi_ci::PropertyReplyData MidiService::propertyGetDataRequested (juce::midi_ci::MUID, const juce::midi_ci::PropertyRequestHeader& header)
{
    if (header.resource == "ResourceList")
    {
        juce::Array<juce::var> resourceList;
        resourceList.add(createDeviceInfoPropertyResourceListEntry());
        resourceList.add(createMPELayoutPropertyResourceListEntry());
        return { {}, juce::midi_ci::Encodings::jsonTo7BitText(std::move(resourceList)) };
    }

    if (header.resource == deviceInfoPropertyResource)
        return createDeviceInfoPropertyReply();

    if (header.resource == mpeLayoutPropertyResource)
    {
        const juce::ScopedLock stateGuard(stateLock_);
        return createMPELayoutPropertyReply(mpeZone_);
    }

    juce::midi_ci::PropertyReplyData result;
    result.header.status = 404;
    result.header.message = "Unable to locate resource " + header.resource;
    return result;
}

juce::midi_ci::PropertyReplyHeader MidiService::propertySetDataRequested (juce::midi_ci::MUID, const juce::midi_ci::PropertyRequestData&)
{
    juce::midi_ci::PropertyReplyHeader h;
    h.status = 404;
    return h;
}

bool MidiService::subscriptionStartRequested (juce::midi_ci::MUID, const juce::midi_ci::PropertySubscriptionHeader&)
{
    return false;
}

void MidiService::subscriptionDidStart (juce::midi_ci::MUID, const juce::String&, const juce::midi_ci::PropertySubscriptionHeader&)
{
}

void MidiService::subscriptionWillEnd (juce::midi_ci::MUID, const juce::midi_ci::Subscription&)
{
}

void MidiService::profileStateReceived (juce::midi_ci::MUID x, juce::midi_ci::ChannelInGroup destination)
{
    if (!initialized_ || !ciDevice_)
        return;

    bool mpeFound = false;
    if (auto* state = ciDevice_->getProfileStateForMuid(x, juce::midi_ci::ChannelAddress().withGroup(0).withChannel(destination)))
    {
        for (auto& profile : state->getActive())
            mpeFound = mpeFound || isMPEProfile(profile);

        for (auto& profile : state->getInactive())
            mpeFound = mpeFound || isMPEProfile(profile);
    }

    if (mpeFound && !remoteSupportsPerNote_)
    {
        remoteSupportsPerNote_ = true;
        if (voiceRouter_)
            voiceRouter_->setRemoteSupportsPerNote(true);
        logMidiExpressionMode();
    }
}

void MidiService::profileEnablementChanged (juce::midi_ci::MUID, juce::midi_ci::ChannelInGroup, juce::midi_ci::Profile profile, int numChannels)
{
    if (isMPEProfile(profile) && numChannels > 0 && !remoteSupportsPerNote_)
    {
        remoteSupportsPerNote_ = true;
        if (voiceRouter_)
            voiceRouter_->setRemoteSupportsPerNote(true);
        logMidiExpressionMode();
    }
}

void MidiService::logMidiExpressionMode()
{
    juce::String mode;
    if (isMidi2Mode_)
        mode = remoteSupportsPerNote_ ? "Native MIDI 2.0 Per-Note Expression" : "MIDI 2.0 with MPE Fallback";
    else
        mode = "MIDI 1.0 (MPE/Channel-per-note)";

    juce::Logger::writeToLog("MidiService: MIDI Expression Mode is now " + mode);
}

void MidiService::sendCISysex (int group, juce::midi_ci::MUID destinationMUID, std::byte subID2, juce::Span<const std::byte> body, std::byte deviceID)
{
    const juce::ScopedLock sl(umpOutputLock_);
    if (!ciDevice_)
        return;

    juce::Logger::writeToLog("MidiService: Sending MIDI-CI SysEx. SubID2=0x" + juce::String::toHexString((int) subID2)
                             + " to MUID=0x" + juce::String::toHexString(destinationMUID.get()));

    std::vector<std::byte> msg;
    // Common MIDI-CI SysEx framing, before JUCE repacks it into UMP:
    //   `0x7E`   = Universal Non-Realtime SysEx
    //   deviceID = SysEx device ID / channel target (`0x7F` would mean broadcast)
    //   `0x0D`   = Sub-ID#1 for MIDI-CI
    //   subID2   = MIDI-CI message type (Discovery, Protocol Negotiation, Profile, PE...)
    //   `0x02`   = MIDI-CI message version / format version used by this implementation
    // Then follow the source and destination MUID values, each packed as four 7-bit
    // bytes least-significant chunk first, followed by the message-specific body.
    // See the MIDI 2.0 / MIDI-CI specification sections covering Universal SysEx,
    // Discovery/CI message headers, and MUID encoding.
    msg.push_back(std::byte { 0x7e });
    msg.push_back(deviceID);
    msg.push_back(std::byte { 0x0d });
    msg.push_back(subID2);
    msg.push_back(std::byte { 0x02 });

    const auto ourMuid = ciDevice_->getMuid().get();
    msg.push_back(std::byte { static_cast<uint8_t>(ourMuid & 0x7f) });
    msg.push_back(std::byte { static_cast<uint8_t>((ourMuid >> 7) & 0x7f) });
    msg.push_back(std::byte { static_cast<uint8_t>((ourMuid >> 14) & 0x7f) });
    msg.push_back(std::byte { static_cast<uint8_t>((ourMuid >> 21) & 0x7f) });

    const auto destMuid = destinationMUID.get();
    msg.push_back(std::byte { static_cast<uint8_t>(destMuid & 0x7f) });
    msg.push_back(std::byte { static_cast<uint8_t>((destMuid >> 7) & 0x7f) });
    msg.push_back(std::byte { static_cast<uint8_t>((destMuid >> 14) & 0x7f) });
    msg.push_back(std::byte { static_cast<uint8_t>((destMuid >> 21) & 0x7f) });

    for (auto byte : body)
        msg.push_back(byte);

    juce::universal_midi_packets::BytesOnGroup bog { static_cast<uint8_t>(group), juce::Span<const std::byte>(msg.data(), msg.size()) };

    auto sendToOutput = [&bog](juce::universal_midi_packets::Output& out) {
        if (!out.isAlive())
            return;

        juce::universal_midi_packets::Conversion::umpFrom7BitData(bog, [&out](const juce::universal_midi_packets::View& view) {
            using namespace juce::universal_midi_packets;
            out.send(Iterator(view.data(), view.size()), Iterator(view.data() + view.size(), 0));
        });
    };

    if (isVirtualTarget_ && isMidi2Mode_)
    {
        if (group >= 0 && static_cast<size_t>(group) < directUmpOutputs_.size())
            sendToOutput(directUmpOutputs_[static_cast<size_t>(group)]);
    }
    else
    {
        sendToOutput(umpOutput_);

        if (isFirstInstance_ && isMidi2Mode_)
            for (auto& directUmpOutput : directUmpOutputs_)
                sendToOutput(directUmpOutput);
    }
}

void MidiService::sendInitiateProtocolNegotiation (int group, juce::midi_ci::MUID destinationMUID, std::byte deviceID)
{
    std::vector<std::byte> body;
    // Protocol Negotiation Initiate message body:
    //   `0x30` = authority level / protocol negotiation function block value used here
    //   `0x01` = number of offered protocols
    //   `0x02 0x00 0x00 0x00 0x00` = the 5-byte MIDI 2.0 protocol identifier in
    //                                MIDI-CI protocol list format
    // The surrounding SysEx header is added by `sendCISysex(..., 0x10, ...)`, where
    // `0x10` is the MIDI-CI Sub-ID#2 for Initiate Protocol Negotiation.
    // See the MIDI-CI specification section on Protocol Negotiation messages and
    // the protocol ID table for the byte layout.
    body.push_back(std::byte { 0x30 });
    body.push_back(std::byte { 0x01 });
    body.push_back(std::byte { 0x02 });
    body.push_back(std::byte { 0x00 });
    body.push_back(std::byte { 0x00 });
    body.push_back(std::byte { 0x00 });
    body.push_back(std::byte { 0x00 });

    sendCISysex(group, destinationMUID, std::byte { 0x10 }, juce::Span<const std::byte>(body.data(), body.size()), deviceID);
}

void MidiService::sendIdentityResponse (int group, std::byte deviceID)
{
    const juce::ScopedLock sl(umpOutputLock_);
    juce::Logger::writeToLog("MidiService: Sending Identity Response.");

    std::vector<std::byte> msg;
    // Standard SysEx Identity Reply payload:
    //   `0x7E` = Universal Non-Realtime SysEx
    //   deviceID = target device ID from the original inquiry
    //   `0x06` = General Information
    //   `0x02` = Identity Reply
    //   `0x7D 0x00 0x00` = manufacturer ID (development / educational ID)
    //   `0x01 0x00`      = family code
    //   `0x01 0x00`      = model number
    //   `0x01 0x00 0x00 0x00` = version / revision bytes
    // For details, see the MIDI 1.0 Detailed Specification section covering the
    // Universal Non-Realtime Identity Request / Identity Reply message format.
    msg.push_back(std::byte { 0x7e });
    msg.push_back(deviceID);
    msg.push_back(std::byte { 0x06 });
    msg.push_back(std::byte { 0x02 });
    msg.push_back(std::byte { 0x7D });
    msg.push_back(std::byte { 0x00 });
    msg.push_back(std::byte { 0x00 });
    msg.push_back(std::byte { 0x01 });
    msg.push_back(std::byte { 0x00 });
    msg.push_back(std::byte { 0x01 });
    msg.push_back(std::byte { 0x00 });
    msg.push_back(std::byte { 0x01 });
    msg.push_back(std::byte { 0x00 });
    msg.push_back(std::byte { 0x00 });
    msg.push_back(std::byte { 0x00 });

    juce::universal_midi_packets::BytesOnGroup bog { static_cast<uint8_t>(group), juce::Span<const std::byte>(msg.data(), msg.size()) };

    auto sendToOutput = [&bog](juce::universal_midi_packets::Output& out) {
        if (!out.isAlive())
            return;

        juce::universal_midi_packets::Conversion::umpFrom7BitData(bog, [&out](const juce::universal_midi_packets::View& view) {
            using namespace juce::universal_midi_packets;
            out.send(Iterator(view.data(), view.size()), Iterator(view.data() + view.size(), 0));
        });
    };

    if (isVirtualTarget_ && isMidi2Mode_)
    {
        if (group >= 0 && static_cast<size_t>(group) < directUmpOutputs_.size())
            sendToOutput(directUmpOutputs_[static_cast<size_t>(group)]);
    }
    else
    {
        sendToOutput(umpOutput_);

        if (isFirstInstance_ && isMidi2Mode_)
            for (auto& directUmpOutput : directUmpOutputs_)
                sendToOutput(directUmpOutput);
    }
}

} // namespace ecm