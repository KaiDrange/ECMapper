#include "StandaloneApp.h"

void ECMapperStandaloneApplication::initialise (const juce::String& commandLine)
{
    juce::ignoreUnused (commandLine);
    mainWindow = std::make_unique<StandaloneAppMainWindow> (getApplicationName());
}

void ECMapperStandaloneApplication::shutdown()
{
    mainWindow = nullptr;
}

void ECMapperStandaloneApplication::systemRequestedQuit()
{
    if (allowQuitWithoutPromptOnce_)
    {
        allowQuitWithoutPromptOnce_ = false;
        juce::JUCEApplication::systemRequestedQuit();
        return;
    }

    if (mainWindow != nullptr)
        StandaloneAppMainWindow::requestQuit();
    else
        quit();
}

void ECMapperStandaloneApplication::anotherInstanceStarted (const juce::String& commandLine)
{
    juce::ignoreUnused (commandLine);
}

START_JUCE_APPLICATION (ECMapperStandaloneApplication)
