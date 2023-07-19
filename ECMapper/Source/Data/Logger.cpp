#include "Logger.h"

ECMLogger::ECMLogger(bool logToFile, bool logToConsole) {
    this->logToFile = logToFile;
    this->logToConsole = logToConsole;
    if (logToFile) {
        logFile = new juce::File(logPath + timeToLogTimeStamp(juce::Time::getCurrentTime()));
    }
}

ECMLogger::~ECMLogger() {
    if (logFile != nullptr)
        delete logFile;
}

void ECMLogger::log(juce::String text) {
    if (logToConsole)
        std::cout << text << std::endl;
    
    if (logToFile) {
        logFile->appendText(timeToLogTimeStamp(juce::Time::getCurrentTime()) + ": ");
        logFile->appendText(text);
        logFile->appendText(juce::NewLine());
    }
}


juce::String ECMLogger::timeToLogTimeStamp(juce::Time time) {
    return juce::String(time.getYear()) + "_"
        + juce::String(time.getMonth()+1) + "_"
        + juce::String(time.getDayOfMonth()) + "_"
        + juce::String(time.getHours()) + "_"
        + juce::String(time.getMinutes()) + "_"
        + juce::String(time.getSeconds()) + "_"
        + juce::String(time.getMilliseconds());
};

