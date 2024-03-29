#include "TabPage.h"

TabPage::TabPage(int tabIndex, DeviceType deviceType, juce::AudioProcessorValueTreeState &pluginState) : keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight) {
    layoutPanel = new LayoutComponent(deviceType, 0.4, 1, pluginState);
    addAndMakeVisible(layoutPanel);
    for (int i = 0; i < 3; i++) {
        zonePanels[i] = new ZonePanelComponent(deviceType, (Zone)(i+1), 1, 1.0/3.0, pluginState);
        addAndMakeVisible(zonePanels[i]);
    }
    
    addAndMakeVisible(keyboard);
    keyboardState.addListener(layoutPanel);
    keyboardState.addListener(&layoutPanel->chordSectionComponent);

    addAndMakeVisible(saveMappingButton);
    saveMappingButton.setButtonText("Save");
    saveMappingButton.onClick = [&, deviceType] {
        FileUtil::saveMapping(LayoutWrapper::getLayoutTree(deviceType, pluginState.state), deviceType);
    };
    addAndMakeVisible(loadMappingButton);
    loadMappingButton.setButtonText("Load");
    loadMappingButton.onClick = [&, deviceType] {
        layoutPanel->deselectAllKeys();
        auto xml = FileUtil::openMapping(deviceType);
        if (xml == "")
            return;
        
        auto loadedMap = juce::ValueTree::fromXml(xml);
        auto oldTree = LayoutWrapper::getLayoutTree(deviceType, pluginState.state);
        oldTree.copyPropertiesFrom(loadedMap, nullptr);
        for (int i = 0; i < oldTree.getNumChildren(); i++) {
            oldTree.getChild(i).copyPropertiesFrom(loadedMap.getChild(i), nullptr);
        }
        repaint();
    };
    addAndMakeVisible(clearMappingButton);
    clearMappingButton.setButtonText("Clear");
    clearMappingButton.onClick = [&, deviceType] {
        layoutPanel->deselectAllKeys();
        auto loadedMap = juce::ValueTree::fromXml("");
        auto oldTree = LayoutWrapper::getLayoutTree(deviceType, pluginState.state);
        oldTree.copyPropertiesFrom(loadedMap, nullptr);
        for (int i = 0; i < oldTree.getNumChildren(); i++) {
            oldTree.getChild(i).copyPropertiesFrom(loadedMap.getChild(i), nullptr);
        }
        repaint();
    };
    
    addAndMakeVisible(controlLightsButton);
    controlLightsButton.setButtonText("Control LEDs");
    controlLightsButton.setToggleState(SettingsWrapper::getControlLights(deviceType, pluginState.state), juce::dontSendNotification);
    controlLightsButton.onClick = [&, deviceType] {
        SettingsWrapper::setControlLights(controlLightsButton.getToggleState(), deviceType, pluginState.state);
    };
    
    addKeyListener(layoutPanel);
}

TabPage::~TabPage() {
    delete layoutPanel;
    for (int i = 0; i < 3; i++) {
        delete zonePanels[i];
    }
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
    clearMappingButton.setBounds(btnarea.removeFromLeft(area.getWidth()*0.1));
    controlLightsButton.setBounds(btnarea.removeFromRight(area.getWidth()*0.15));

    layoutPanel->setBounds(area.removeFromLeft(area.getWidth()*layoutPanel->widthFactor));
    
    auto zoneArea = area.removeFromRight(area.getWidth()*0.98);
    for (int i = 0; i < 3; i++) {
        zonePanels[i]->setBounds(zoneArea.removeFromTop(area.getHeight()*zonePanels[i]->heightFactor));
    }
}
