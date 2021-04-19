#include <JuceHeader.h>
#include "TabPage.h"

TabPage::TabPage(InstrumentType model)
{
    keymap = new EigenharpMapping(model);
    keymapPanel = new KeymapPanelComponent(keymap, 0.35, 1);
    addAndMakeVisible(keymapPanel);
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
    keymapPanel->setBounds(area.removeFromLeft(area.getWidth()*keymapPanel->widthFactor));
}
