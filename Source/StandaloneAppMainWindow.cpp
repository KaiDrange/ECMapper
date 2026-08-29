#include "StandaloneAppMainWindow.h"

StandaloneAppMainWindow::StandaloneAppMainWindow (const juce::String& name)
    : DocumentWindow (name,
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                  .findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    centreWithSize (800, 600);
    setVisible (true);
}

StandaloneAppMainWindow::~StandaloneAppMainWindow()
{
}

void StandaloneAppMainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
