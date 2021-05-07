#include "MainComponent.h"

MainComponent::MainComponent() : tabs(juce::TabbedButtonBar::TabsAtTop) {
    tabPages[0] = new TabPage(InstrumentType::Alpha);
    tabPages[1] = new TabPage(InstrumentType::Tau);
    tabPages[2] = new TabPage(InstrumentType::Pico);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
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
