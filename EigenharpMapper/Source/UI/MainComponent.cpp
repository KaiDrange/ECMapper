#include "MainComponent.h"

MainComponent::MainComponent() : tabs(juce::TabbedButtonBar::TabsAtTop), uiSettings("uiSettings") {
    tabPages[0] = new TabPage(InstrumentType::Alpha);
    tabPages[1] = new TabPage(InstrumentType::Tau);
    tabPages[2] = new TabPage(InstrumentType::Pico);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
    addAndMakeVisible(tabs);
    
    uiSettings.addChild(tabPages[0]->getEigenharpMapping()->getValueTree(), 0, nullptr);
    uiSettings.addChild(tabPages[1]->getEigenharpMapping()->getValueTree(), 1, nullptr);
    uiSettings.addChild(tabPages[2]->getEigenharpMapping()->getValueTree(), 2, nullptr);

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

juce::ValueTree* MainComponent::getValueTree() {
    return &uiSettings;
}

