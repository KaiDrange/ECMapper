#include "StandaloneAppMainWindow.h"

StandaloneAppMainWindow::StandaloneAppMainWindow (const juce::String& name)
    : DocumentWindow (name,
                      ecm::Style::background(),
                      DocumentWindow::allButtons)
{
    setLookAndFeel(&lookAndFeel);
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    
    processor = std::make_unique<ECMapperAudioProcessor>();
    processor->setDeviceManager(&deviceManager);
    
    processorPlayer.setProcessor(processor.get());
    
    loadAudioSettings();
    
    deviceManager.addAudioCallback(&processorPlayer);
    
    updateMidiOutput();
    
    loadPluginState();
    
    deviceManager.addChangeListener(this);
    
    setContentOwned(processor->createEditor(), true);

    centreWithSize (1000, 700);
    setVisible (true);
    
    saveAudioSettings();
}

StandaloneAppMainWindow::~StandaloneAppMainWindow()
{
    setContentOwned(nullptr, true);
    setLookAndFeel(nullptr);
    savePluginState();
    deviceManager.removeChangeListener(this);
    processorPlayer.setProcessor(nullptr);
    deviceManager.removeAudioCallback(&processorPlayer);
}

void StandaloneAppMainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void StandaloneAppMainWindow::changeListenerCallback (juce::ChangeBroadcaster* source)
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
    
    if (currentOutput != nullptr) {
        juce::Logger::writeToLog("ECMapper: MIDI Output set to: " + currentOutput->getName());
    } else {
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
            file.getParentDirectory().createDirectory();
            
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

void StandaloneAppMainWindow::savePluginState()
{
    juce::MemoryBlock data;
    processor->getStateInformation(data);
    auto file = getPluginStateFile();
    if (!file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();
    
    if (file.replaceWithData(data.getData(), data.getSize()))
        juce::Logger::writeToLog("ECMapper: Saved plugin state to " + file.getFullPathName());
}

void StandaloneAppMainWindow::loadPluginState()
{
    auto file = getPluginStateFile();
    if (file.existsAsFile())
    {
        juce::MemoryBlock data;
        if (file.loadFileAsData(data))
        {
            processor->setStateInformation(data.getData(), (int)data.getSize());
            juce::Logger::writeToLog("ECMapper: Loaded plugin state from " + file.getFullPathName());
        }
    }
}

juce::File StandaloneAppMainWindow::getAudioSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ECMapper")
        .getChildFile("audio_settings.xml");
}

juce::File StandaloneAppMainWindow::getPluginStateFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ECMapper")
        .getChildFile("plugin_state.xml");
}
