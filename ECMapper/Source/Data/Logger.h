#pragma once

#include <JuceHeader.h>

class ECMLogger {
public:
    ECMLogger(bool logToFile, bool logToConsole);
    ~ECMLogger();
    void log(juce::String text);
    
private:
    bool logToFile;
    bool logToConsole;
    juce::String logPath = "~/Documents/ECMapperLogs/";
    juce::File *logFile = nullptr;
    
    juce::String timeToLogTimeStamp(juce::Time time);
};
