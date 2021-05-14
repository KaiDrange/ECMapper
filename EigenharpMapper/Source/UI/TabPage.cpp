#include "TabPage.h"

TabPage::TabPage(InstrumentType model) : keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight) {
    keymap = new EigenharpMapping(model);
    keymapPanel = new KeymapPanelComponent(keymap, 0.4, 1);
    addAndMakeVisible(keymapPanel);
    for (int i = 0; i < 3; i++) {
        zonePanels[i] = new ZonePanelComponent(i+1, 1, 1.0/3.0);
        addAndMakeVisible(zonePanels[i]);
    }
    
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
    
    addAndMakeVisible(oscIPInput);
    oscIPInput.setText("localhost:7001");
    
    addKeyListener(keymapPanel);
}

TabPage::~TabPage() {
    delete keymapPanel;
    for (int i = 0; i < 3; i++) {
        delete zonePanels[i];
    }
    delete keymap;
}

void TabPage::paint(juce::Graphics& g) {
}

void TabPage::resized() {
    auto area = getLocalBounds();
    area.reduce(5, 5);
    keyboard.setBounds(area.removeFromLeft(area.getWidth()*0.1));
    area.removeFromLeft(area.getWidth()*0.01);
    auto btnarea = area.removeFromTop(area.getHeight()*0.03);
    loadMappingButton.setBounds(btnarea.removeFromLeft(area.getWidth()*0.1));
    saveMappingButton.setBounds(btnarea.removeFromLeft(area.getWidth()*0.1));
    oscIPInput.setBounds(btnarea.removeFromRight(area.getWidth()*0.2));

    keymapPanel->setBounds(area.removeFromLeft(area.getWidth()*keymapPanel->widthFactor));
    
    auto zoneArea = area.removeFromRight(area.getWidth()*0.98);
    for (int i = 0; i < 3; i++) {
        zonePanels[i]->setBounds(zoneArea.removeFromTop(area.getHeight()*zonePanels[i]->heightFactor));
    }
}

EigenharpMapping* TabPage::getEigenharpMapping() {
    return keymap;
}


