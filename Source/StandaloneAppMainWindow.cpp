#include "StandaloneAppMainWindow.h"

#include "UI/AboutDialogComponent.h"
#include "StandaloneApp.h"
#include "UI/PresetBrowserComponent.h"

StandaloneAppMainWindow::StandaloneAppMainWindow(const juce::String& name)
    : DocumentWindow(name,
                     ecm::Style::background(),
                     DocumentWindow::allButtons),
      menuBarModel(
          [] { requestQuit(); },
          nullptr,
          [this] { showPresetBrowser(); },
          [this] { showAudioSettings(); },
          [this] { showAboutDialog(); },
          [] { showOnlineManual(); },
          [] { showOurMusic(); }
      )
{
    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    setUsingNativeTitleBar(true);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(&menuBarModel);
#endif

#if ! JUCE_MAC
    setMenuBar(&menuBarModel);
#endif
    setResizable(true, true);

    processor = std::make_unique<ECMapperAudioProcessor>();
    processor->setDeviceManager(&deviceManager);
    processor->loadStandalonePresetBank();

    processorPlayer.setProcessor(processor.get());

    loadAudioSettings();
    processor->loadPresetSlot(1);

    deviceManager.addAudioCallback(&processorPlayer);
    deviceManager.addMidiInputDeviceCallback({}, &processorPlayer.getMidiMessageCollector());

    updateMidiOutput();

    deviceManager.addChangeListener(this);

    setContentOwned(processor->createUI(), true);

    centreWithSize(1000, 700);
    Component::setVisible(true);

    saveAudioSettings();
}

StandaloneAppMainWindow::~StandaloneAppMainWindow()
{
    setContentOwned(nullptr, true);
    setLookAndFeel(nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    deviceManager.removeChangeListener(this);
    processorPlayer.setProcessor(nullptr);
    deviceManager.removeMidiInputDeviceCallback({}, &processorPlayer.getMidiMessageCollector());
    deviceManager.removeAudioCallback(&processorPlayer);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif

    setMenuBar(nullptr);
}

void StandaloneAppMainWindow::closeButtonPressed()
{
    requestQuit();
}

void StandaloneAppMainWindow::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager && !isUpdatingSettings)
    {
        isUpdatingSettings = true;
        updateMidiOutput();
        saveAudioSettings();
        isUpdatingSettings = false;
    }
}

void StandaloneAppMainWindow::updateMidiOutput()
{
    auto* currentOutput = deviceManager.getDefaultMidiOutput();

    if (currentOutput != nullptr)
    {
        juce::Logger::writeToLog("ECMapper: MIDI Output set to: " + currentOutput->getName());
    }
    else
    {
        juce::Logger::writeToLog("ECMapper: No MIDI Output selected.");
    }

    processorPlayer.setMidiOutput(currentOutput);
}

void StandaloneAppMainWindow::saveAudioSettings()
{
    auto xml = deviceManager.createStateXml();
    if (xml != nullptr)
    {
        auto file = getAudioSettingsFile();
        if (!file.getParentDirectory().exists())
            // ReSharper disable once CppExpressionWithoutSideEffects
            file.getParentDirectory().createDirectory();

        // ReSharper disable once CppExpressionWithoutSideEffects
        xml->writeTo(file);
    }
}

void StandaloneAppMainWindow::loadAudioSettings()
{
    auto file = getAudioSettingsFile();
    if (file.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr)
        {
            juce::Logger::writeToLog("ECMapper: Loading audio settings from " + file.getFullPathName());
            deviceManager.initialise(0, 2, xml.get(), true);
            return;
        }
    }

    juce::Logger::writeToLog("ECMapper: Initializing with default audio devices.");
    deviceManager.initialiseWithDefaultDevices(0, 2);
}

juce::File StandaloneAppMainWindow::getAudioSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("ECMapper")
           .getChildFile("audio_settings.xml");
}

void StandaloneAppMainWindow::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager,
                                                            0, 0, 0, 256, true, true, true, false);
    selector->setSize(500, 450);
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Audio/MIDI Settings";
    options.dialogBackgroundColour = ecm::Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void StandaloneAppMainWindow::showPresetBrowser()
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ecm::PresetBrowserComponent(*processor));
    options.dialogTitle = "Presets";
    options.dialogBackgroundColour = ecm::Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = this;
    options.launchAsync();
}

void StandaloneAppMainWindow::requestQuit()
{
    if (auto* app = dynamic_cast<ECMapperStandaloneApplication*>(juce::JUCEApplication::getInstance()))
        app->allowQuitWithoutPromptOnce();

    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void StandaloneAppMainWindow::showAboutDialog()
{
    auto about = std::make_unique<AboutDialogComponent>(
        "ECMapper",
        ProjectInfo::versionString
    );

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(about.release());
    options.dialogTitle = "About ECMapper";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;

    options.launchAsync();
}

void StandaloneAppMainWindow::showOnlineManual()
{
    juce::ignoreUnused(juce::URL(AboutDialogComponent::onlineManualUrl).launchInDefaultBrowser());
}

void StandaloneAppMainWindow::showOurMusic()
{
    juce::ignoreUnused(juce::URL(AboutDialogComponent::ourMusicUrl).launchInDefaultBrowser());
}
