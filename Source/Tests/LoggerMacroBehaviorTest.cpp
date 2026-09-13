#include "Core/Logger.h"

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

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    int defaultLoggerArgumentEvaluations = 0;
    int explicitLoggerArgumentEvaluations = 0;

    [[maybe_unused]] auto makeDefaultLogMessage = [&]() {
        ++defaultLoggerArgumentEvaluations;
        return juce::String("default logger message");
    };

    [[maybe_unused]] auto makeExplicitLogMessage = [&]() {
        ++explicitLoggerArgumentEvaluations;
        return juce::String("explicit logger message");
    };

    ecm::Logger logger(false, false);
    ECM_LOG(makeDefaultLogMessage());
    ECM_LOGGER(logger, makeExplicitLogMessage());

#if JUCE_DEBUG
    const bool ok = expect(defaultLoggerArgumentEvaluations == 1,
                           "ECM_LOG should evaluate its message in Debug builds")
        && expect(explicitLoggerArgumentEvaluations == 1,
                  "ECM_LOGGER should evaluate its message in Debug builds");
#else
    const bool ok = expect(defaultLoggerArgumentEvaluations == 0,
                           "ECM_LOG should compile out its message in Release builds")
        && expect(explicitLoggerArgumentEvaluations == 0,
                  "ECM_LOGGER should compile out its message in Release builds");
#endif

    if (!ok)
        return 1;

    std::cout << "LoggerMacroBehaviorTest passed" << std::endl;
    return 0;
}