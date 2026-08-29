#include "StandaloneAppMainWindow.h"

StandaloneAppMainWindow::StandaloneAppMainWindow (const juce::String& name)
    : DocumentWindow (name,
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                  .findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    
    processor = std::make_unique<ECMapperAudioProcessor>();
    
    deviceManager.initialiseWithDefaultDevices(0, 2);
    deviceManager.addAudioCallback(&processorPlayer);
    processorPlayer.setProcessor(processor.get());
    
    setContentOwned(processor->createEditor(), true);

    centreWithSize (1000, 700);
    setVisible (true);
}

StandaloneAppMainWindow::~StandaloneAppMainWindow()
{
    processorPlayer.setProcessor(nullptr);
    deviceManager.removeAudioCallback(&processorPlayer);
}

void StandaloneAppMainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
