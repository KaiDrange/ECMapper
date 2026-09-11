#define private public
#include "PluginProcessor.h"
#undef private

#include "Core/ZoneWrapper.h"

#include <cmath>
#include <iostream>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

int getTransposeValue(ECMapperAudioProcessor& processor, ecm::InstrumentType deviceType, ecm::Zone zone)
{
    const auto parameterId = ecm::ZoneWrapper::getTransposeParameterID(deviceType, zone);
    if (const auto* raw = processor.state.getRawParameterValue(parameterId))
        return static_cast<int>(std::lround(raw->load()));

    return 0;
}

bool applyTransposeMessage(ECMapperAudioProcessor& processor, int channel, int ccValue)
{
    juce::MidiBuffer midiMessages;
    midiMessages.addEvent(juce::MidiMessage::controllerEvent(channel, 22, ccValue), 0);
    return processor.applyZoneControlMessages(midiMessages);
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    ECMapperAudioProcessor processor;

    bool ok = true;

    ok &= expect(applyTransposeMessage(processor, 1, 0),
                 "transpose CC should report a change when it updates zone 1");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == -64,
                 "CC 22 value 0 should map to -64 semitones");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == 0,
                 "channel 1 transpose CC should not affect zone 2");

    ok &= expect(applyTransposeMessage(processor, 2, 63),
                 "transpose CC should report a change when it updates zone 2");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == -1,
                 "CC 22 value 63 should map to -1 semitone");

    ok &= expect(!applyTransposeMessage(processor, 3, 64),
                 "transpose CC should not report a change when value 64 keeps zone 3 at 0 semitones");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone3) == 0,
                 "CC 22 value 64 should map to 0 semitones");

    ok &= expect(applyTransposeMessage(processor, 4, 65),
                 "channel 4 transpose CC should report a change when it updates all zones");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == 1
                 && getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone2) == 1
                 && getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone3) == 1,
                 "CC 22 value 65 on channel 4 should map to +1 semitone for all zones");

    ok &= expect(applyTransposeMessage(processor, 4, 127),
                 "channel 4 transpose CC should report a change at the top of the range");
    ok &= expect(getTransposeValue(processor, ecm::InstrumentType::Alpha, ecm::Zone::Zone1) == 63
                 && getTransposeValue(processor, ecm::InstrumentType::Tau, ecm::Zone::Zone2) == 63
                 && getTransposeValue(processor, ecm::InstrumentType::Pico, ecm::Zone::Zone3) == 63,
                 "CC 22 value 127 should map to +63 semitones across all devices and zones");

    if (!ok)
        return 1;

    std::cout << "StandaloneInputMidiTransposeTest passed" << std::endl;
    return 0;
}