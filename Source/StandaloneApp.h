#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "StandaloneAppMainWindow.h"

class ECMapperStandaloneApplication : public juce::JUCEApplication
{
public:
    ECMapperStandaloneApplication() = default;

    const juce::String getApplicationName() override { return "ECMapper"; }
    const juce::String getApplicationVersion() override { return "2.0.0-alpha"; }

    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted (const juce::String& commandLine) override;

private:
    std::unique_ptr<StandaloneAppMainWindow> mainWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperStandaloneApplication)
};
