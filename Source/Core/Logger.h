#pragma once
#include <JuceHeader.h>

namespace ecm {

class Logger {
public:
    Logger(bool logToFile, bool logToConsole);
    ~Logger() = default;
    
    void log(const juce::String& text);
    
private:
    bool logToFile_;
    bool logToConsole_;
    juce::File logFile_;
    
    juce::String timeToLogTimeStamp(juce::Time time);
};

} // namespace ecm
