#include "MainComponent.h"

MainComponent::MainComponent(juce::ValueTree state): tabs(juce::TabbedButtonBar::TabsAtTop) {
    uiSettings = state.getOrCreateChildWithName(id_uiSettings, nullptr);

    tabPages[0] = new TabPage(0, DeviceType::Alpha, uiSettings);
    tabPages[1] = new TabPage(1, DeviceType::Tau, uiSettings);
    tabPages[2] = new TabPage(2, DeviceType::Pico, uiSettings);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
    tabs.setCurrentTabIndex(2);
    addAndMakeVisible(tabs);
    
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
