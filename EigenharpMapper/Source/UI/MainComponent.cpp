#include "MainComponent.h"

MainComponent::MainComponent(juce::ValueTree state): tabs(juce::TabbedButtonBar::TabsAtTop) {
    uiSettings = state.getOrCreateChildWithName(id_uiSettings, nullptr);

    //    state.getChildWithName(id_uiSettings);
//    if (!uiSettings.isValid()) {
//        juce::ValueTree newTree(id_uiSettings);
//        state.addChild(newTree, state.getNumChildren(), nullptr);
//        uiSettings = newTree;
//    }
    tabPages[0] = new TabPage(DeviceType::Alpha, uiSettings);
    tabPages[1] = new TabPage(DeviceType::Tau, uiSettings);
    tabPages[2] = new TabPage(DeviceType::Pico, uiSettings);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
    tabs.setCurrentTabIndex(2);
    addAndMakeVisible(tabs);
    
//    uiSettings.addChild(tabPages[0]->getLayout()->getValueTree(), 0, nullptr);
//    uiSettings.addChild(tabPages[1]->getLayout()->getValueTree(), 1, nullptr);
//    uiSettings.addChild(tabPages[2]->getLayout()->getValueTree(), 2, nullptr);

//    for (int i = 0; i < 3; i++) {
//        uiSettings.addChild(tabPages[i]->getZoneConfig(Zone::Zone1)->getValueTree(), 3 + (i*3), nullptr);
//        uiSettings.addChild(tabPages[i]->getZoneConfig(Zone::Zone2)->getValueTree(), 4 + (i*3), nullptr);
//        uiSettings.addChild(tabPages[i]->getZoneConfig(Zone::Zone3)->getValueTree(), 5 + (i*3), nullptr);
//    }

    resized();
}

MainComponent::~MainComponent() {
    delete tabPages[0];
    delete tabPages[1];
    delete tabPages[2];
}

void MainComponent::paint (juce::Graphics& g) {
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    tabs.setBounds(area);
    
}

Layout* MainComponent::getLayout(DeviceType deviceType) {
        return tabPages[(int)deviceType-1]->getLayout();
}

void MainComponent::addListener(juce::ValueTree::Listener *listener) {
    uiSettings.addListener(listener);
}
