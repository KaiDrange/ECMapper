#include <JuceHeader.h>
#include "TabPage.h"

TabPage::TabPage(InstrumentType model) : keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight)
{
    keymap = new EigenharpMapping(model);
    keymapPanel = new KeymapPanelComponent(keymap, 0.35, 1);
    addAndMakeVisible(keymapPanel);
    
    addAndMakeVisible(keyboard);
    keyboardState.addListener(keymapPanel);
}

TabPage::~TabPage()
{
    delete keymapPanel;
    delete keymap;
}

void TabPage::paint (juce::Graphics& g)
{
}

void TabPage::resized()
{
    auto area = getLocalBounds();
    area.reduce(5, 5);
    keyboard.setBounds(area.removeFromLeft(area.getWidth()*0.1));
    area.removeFromLeft(area.getWidth()*0.01);
    keymapPanel->setBounds(area.removeFromLeft(area.getWidth()*keymapPanel->widthFactor));
}

