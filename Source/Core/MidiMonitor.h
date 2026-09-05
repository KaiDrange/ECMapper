#pragma once
#include <JuceHeader.h>
#include <deque>
#include <atomic>

namespace ecm {

class MidiMonitor {
public:
    static MidiMonitor& getInstance() {
        static MidiMonitor instance;
        return instance;
    }

    void addMessage(const juce::String& deviceName, bool isMidi2, const juce::String& description) {
        juce::String protocol = isMidi2 ? "MIDI 2.0" : "MIDI 1.0";
        juce::String timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S.");
        timestamp += juce::String(juce::Time::getMillisecondCounter() % 1000).paddedLeft('0', 3);
        
        juce::String fullMessage = timestamp + " - " + deviceName + " - " + protocol + " - " + description;
        
        const juce::ScopedLock sl(lock);
        messages.push_back(fullMessage);
        if (messages.size() > 500)
            messages.pop_front();
        
        hasNewMessages = true;
    }

    juce::StringArray getMessages() {
        const juce::ScopedLock sl(lock);
        juce::StringArray result;
        for (const auto& m : messages)
            result.add(m);
        return result;
    }

    bool checkAndResetNewMessages() {
        return hasNewMessages.exchange(false);
    }

    void clear() {
        const juce::ScopedLock sl(lock);
        messages.clear();
        hasNewMessages = true;
    }

private:
    MidiMonitor() = default;
    juce::CriticalSection lock;
    std::deque<juce::String> messages;
    std::atomic<bool> hasNewMessages{ false };
};

}
