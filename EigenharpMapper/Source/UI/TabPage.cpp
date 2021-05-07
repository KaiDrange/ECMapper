#include "TabPage.h"

TabPage::TabPage(InstrumentType model) : keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight) {
    keymap = new EigenharpMapping(model);
    keymapPanel = new KeymapPanelComponent(keymap, 0.4, 1);
    addAndMakeVisible(keymapPanel);
    
    addAndMakeVisible(keyboard);
    keyboardState.addListener(keymapPanel);

    addAndMakeVisible(saveMappingButton);
    saveMappingButton.setButtonText("Save");
    saveMappingButton.onClick = [&] {
        FileUtil::saveMapping(keymap->getValueTree(), model);
    };
    addAndMakeVisible(loadMappingButton);
    loadMappingButton.setButtonText("load");
    loadMappingButton.onClick = [&] {
        auto xml = FileUtil::openMapping(model);
        auto loadedMap = juce::ValueTree::fromXml(xml);
        keymap->setValueTree(loadedMap);
        repaint();
    };

    
    addKeyListener(keymapPanel);
}

TabPage::~TabPage() {
    delete keymapPanel;
    delete keymap;
}

void TabPage::paint (juce::Graphics& g) {
}

void TabPage::resized() {
    auto area = getLocalBounds();
    area.reduce(5, 5);
    keyboard.setBounds(area.removeFromLeft(area.getWidth()*0.1));
    area.removeFromLeft(area.getWidth()*0.01);
    auto btnarea = area.removeFromTop(area.getHeight()*0.03);
    loadMappingButton.setBounds(btnarea.removeFromLeft(area.getWidth()*0.1));
    saveMappingButton.setBounds(btnarea.removeFromLeft(area.getWidth()*0.1));
    keymapPanel->setBounds(area.removeFromLeft(area.getWidth()*keymapPanel->widthFactor));
}

