#include "MainComponent.h"

MainComponent::MainComponent() : tabs(juce::TabbedButtonBar::TabsAtTop), uiSettings("uiSettings") {
    tabPages[0] = new TabPage(DeviceType::Alpha);
    tabPages[1] = new TabPage(DeviceType::Tau);
    tabPages[2] = new TabPage(DeviceType::Pico);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
    tabs.setCurrentTabIndex(2);
    addAndMakeVisible(tabs);
    
    uiSettings.addChild(tabPages[0]->getLayout()->getValueTree(), 0, nullptr);
    uiSettings.addChild(tabPages[1]->getLayout()->getValueTree(), 1, nullptr);
    uiSettings.addChild(tabPages[2]->getLayout()->getValueTree(), 2, nullptr);

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

Layout* MainComponent::getLayout(DeviceType::DeviceType deviceType) {
        return tabPages[(int)deviceType-1]->getLayout();
}

