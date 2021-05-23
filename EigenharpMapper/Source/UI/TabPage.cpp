#include "TabPage.h"

TabPage::TabPage(DeviceType model, juce::ValueTree parentTree) : keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight) {
//    keymap = new Layout(model, parentTree);
    keymapPanel = new KeymapPanelComponent(model, 0.4, 1, parentTree);
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
        FileUtil::saveMapping(keymapPanel->layout.getValueTree(), model);
    };
    addAndMakeVisible(loadMappingButton);
    loadMappingButton.setButtonText("load");
    loadMappingButton.onClick = [&] {
        auto xml = FileUtil::openMapping(model);
        auto loadedMap = juce::ValueTree::fromXml(xml);
        std::cout << loadedMap.getType().toString() << std::endl;
        std::cout << loadedMap.getChild(0).getProperty("keyColour").toString() << std::endl;
        
        auto oldTree = keymapPanel->layout.getValueTree();
        oldTree.copyPropertiesFrom(loadedMap, nullptr);
        for (int i = 0; i < oldTree.getNumChildren(); i++) {
            oldTree.getChild(i).copyPropertiesFrom(loadedMap.getChild(i), nullptr);
        }
        
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
//    delete keymap;
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

Layout* TabPage::getLayout() {
    return &keymapPanel->layout;
//    return keymap;
}

ZoneConfig* TabPage::getZoneConfig(Zone zone) {
    return zonePanels[((int)zone) - 1]->getZoneConfig();
}


