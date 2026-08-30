#include "Logger.h"
#include <iostream>

namespace ecm {

Logger::Logger(bool logToFile, bool logToConsole)
    : logToFile_(logToFile), logToConsole_(logToConsole) {
    if (logToFile_) {
        juce::File logDir("~/Documents/ECMapperLogs/");
        logDir.createDirectory();
        logFile_ = logDir.getChildFile(timeToLogTimeStamp(juce::Time::getCurrentTime()) + ".log");
    }
}

void Logger::log(const juce::String& text) {
    const juce::ScopedLock sl(lock_);
    if (logToConsole_) {
        std::cout << text << std::endl;
    }
    
    if (logToFile_) {
        logFile_.appendText(timeToLogTimeStamp(juce::Time::getCurrentTime()) + ": " + text + juce::NewLine());
    }
}

juce::String Logger::timeToLogTimeStamp(juce::Time time) {
    return juce::String(time.getYear()) + "_"
        + juce::String(time.getMonth() + 1) + "_"
        + juce::String(time.getDayOfMonth()) + "_"
        + juce::String(time.getHours()) + "_"
        + juce::String(time.getMinutes()) + "_"
        + juce::String(time.getSeconds()) + "_"
        + juce::String(time.getMilliseconds());
}

} // namespace ecm
