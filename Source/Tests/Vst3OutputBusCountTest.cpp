#include <JuceHeader.h>

#include <iostream>
#include <string_view>

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

int parseConfiguredBusCount(const juce::String& source)
{
    constexpr std::string_view definePrefix = "#define ECMAPPER_VST3_DIRECT_OUTPUT_BUS_COUNT ";
    const auto sourceView = std::string_view(source.toRawUTF8(), static_cast<size_t>(source.getNumBytesAsUTF8()));
    const auto definePosition = sourceView.find(definePrefix);

    if (definePosition == std::string_view::npos)
        return -1;

    const auto valueStart = definePosition + definePrefix.size();
    auto valueEnd = valueStart;
    while (valueEnd < sourceView.size() && sourceView[valueEnd] >= '0' && sourceView[valueEnd] <= '9')
        ++valueEnd;

    return juce::String(sourceView.substr(valueStart, valueEnd - valueStart).data(),
                        valueEnd - valueStart).getIntValue();
}

} // namespace

int main()
{
    const auto sourceRoot = juce::File(juce::String{ ECMAPPER_SOURCE_DIR });
    const auto wrapperFile = sourceRoot.getChildFile("JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp");

    bool ok = true;
    ok &= expect(wrapperFile.existsAsFile(), "the VST3 client wrapper source should exist");

    const auto wrapperSource = wrapperFile.loadFileAsString();
    const auto configuredBusCount = parseConfiguredBusCount(wrapperSource);

    ok &= expect(configuredBusCount == 3,
                 "the VST3 plugin wrapper should advertise three MIDI output buses so hosts expose 48 output channels");
    ok &= expect(wrapperSource.contains("MIDI Output 3"),
                 "the VST3 plugin wrapper should still describe the third MIDI output bus");

    return ok ? 0 : 1;
}