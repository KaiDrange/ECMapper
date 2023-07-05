#pragma once

#include <JuceHeader.h>

class Logger {
public:
    Logger(bool logToFile, bool logToConsole);
    ~Logger();
    void log(juce::String text);
    
private:
    bool logToFile;
    bool logToConsole;
    juce::String logPath = "~/Documents/ECMapperLogs/";
    juce::File *logFile = nullptr;
    
    juce::String timeToLogTimeStamp(juce::Time time);
};
