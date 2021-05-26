#include "MainComponent.h"

MainComponent::MainComponent(juce::ValueTree state): lowMPEChannelCount("Low MPE chan to:", 2, 0, 16, 16, true), lowMPEPitchbendRange("Low MPE pb:", 2, 0, 96, 48, true), highMPEPitchbendRange("High MPE pb:", 2, 0, 96, 48, true), tabs(juce::TabbedButtonBar::TabsAtTop) {
    
    SettingsWrapper::addListener(this);
    oscIPInput.setText(SettingsWrapper::getIP());
    oscIPInput.onFocusLost = [&] {
        SettingsWrapper::setIP(oscIPInput.getText());
    };
    
    lowMPEChannelCount.setValue(SettingsWrapper::getLowMPEToChannel());
    lowMPEChannelCount.input.onFocusLost = [&] {
        SettingsWrapper::setLowMPEToChannel(lowMPEChannelCount.getValue());
    };
    
    lowMPEPitchbendRange.setValue(SettingsWrapper::getLowMPEPB());
    lowMPEPitchbendRange.input.onFocusLost = [&] {
        SettingsWrapper::setLowMPEPB(lowMPEPitchbendRange.getValue());
    };

    highMPEPitchbendRange.setValue(SettingsWrapper::getHighMPEPB());
    highMPEPitchbendRange.input.onFocusLost = [&] {
        SettingsWrapper::setHighMPEPB(highMPEPitchbendRange.getValue());
    };
    
    uiSettings = state.getOrCreateChildWithName(id_uiSettings, nullptr);
    
    tabPages[0] = new TabPage(0, DeviceType::Alpha, uiSettings);
    tabPages[1] = new TabPage(1, DeviceType::Tau, uiSettings);
    tabPages[2] = new TabPage(2, DeviceType::Pico, uiSettings);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
    tabs.setCurrentTabIndex(2);
    
    oscIPLabel.setText("Core IP:", juce::dontSendNotification);
    addAndMakeVisible(tabs);
    addAndMakeVisible(oscIPLabel);
    addAndMakeVisible(oscIPInput);
    addAndMakeVisible(lowMPEChannelCount);
    addAndMakeVisible(lowMPEPitchbendRange);
    addAndMakeVisible(highMPEPitchbendRange);
    
    resized();
}

MainComponent::~MainComponent() {
    delete tabPages[0];
    delete tabPages[1];
    delete tabPages[2];
}

void MainComponent::paint (juce::Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    auto header = getLocalBounds().removeFromTop(30);
    header.removeFromTop(5);
    
    auto ipArea = header.removeFromRight(area.getWidth()*0.2);
    oscIPLabel.setBounds(ipArea.removeFromTop(ipArea.getHeight()*0.5));
    oscIPInput.setBounds(ipArea);
    header.removeFromRight(area.getWidth()*0.02);
    highMPEPitchbendRange.setBounds(header.removeFromRight(area.getWidth()*0.12));
    header.removeFromRight(area.getWidth()*0.02);
    lowMPEPitchbendRange.setBounds(header.removeFromRight(area.getWidth()*0.12));
    header.removeFromRight(area.getWidth()*0.02);
    lowMPEChannelCount.setBounds(header.removeFromRight(area.getWidth()*0.12));
    tabs.setBounds(area);
    
}

//Layout* MainComponent::getLayout(DeviceType deviceType) {
//        return tabPages[(int)deviceType-1]->getLayout();
//}

void MainComponent::addListener(juce::ValueTree::Listener *listener) {
    uiSettings.addListener(listener);
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
}
