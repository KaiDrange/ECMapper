#pragma once
#include <JuceHeader.h>

namespace ecm {

class Logger {
public:
    Logger(bool logToFile, bool logToConsole);
    ~Logger() = default;

    static Logger& getDefault();
    
    void log(const juce::String& text);
    
private:
    juce::CriticalSection lock_;
    bool logToFile_;
    bool logToConsole_;
    juce::File logFile_;
    
    juce::String timeToLogTimeStamp(juce::Time time);
};

} // namespace ecm

#if JUCE_DEBUG
    #define ECM_LOG(text) do { ::ecm::Logger::getDefault().log((text)); } while (false)
    #define ECM_LOGGER(logger, text) do { (logger).log((text)); } while (false)
#else
    #define ECM_LOG(text) do {} while (false)
    #define ECM_LOGGER(logger, text) do {} while (false)
#endif
