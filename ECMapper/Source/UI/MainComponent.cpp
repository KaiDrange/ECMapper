#include "MainComponent.h"

MainComponent::MainComponent(): lowerMPEVoiceCount("Lower MPE voices:", 2, 0, 15, true), upperMPEVoiceCount("Upper MPE voices:", 2, 0, 15, true),  lowerMPEPitchbendRange("Lower MPE pb:", 2, 0, 96, true), upperMPEPitchbendRange("Upper MPE pb:", 2, 0, 96, true), tabs(juce::TabbedButtonBar::TabsAtTop) {
    
    SettingsWrapper::addListener(this);
    oscIPInput.setText(SettingsWrapper::getIP());
    oscIPInput.onFocusLost = [&] {
        SettingsWrapper::setIP(oscIPInput.getText());
    };
    
    lowerMPEVoiceCount.setValue(SettingsWrapper::getLowerMPEVoiceCount());
    lowerMPEVoiceCount.input.onFocusLost = [&] {
        SettingsWrapper::setLowerMPEVoiceCount(lowerMPEVoiceCount.getValue());
    };

    upperMPEVoiceCount.setValue(SettingsWrapper::getUpperMPEVoiceCount());
    upperMPEVoiceCount.input.onFocusLost = [&] {
        SettingsWrapper::setUpperMPEVoiceCount(upperMPEVoiceCount.getValue());
    };
    
    lowerMPEPitchbendRange.setValue(SettingsWrapper::getLowerMPEPB());
    lowerMPEPitchbendRange.input.onFocusLost = [&] {
        SettingsWrapper::setLowerMPEPB(lowerMPEPitchbendRange.getValue());
    };

    upperMPEPitchbendRange.setValue(SettingsWrapper::getUpperMPEPB());
    upperMPEPitchbendRange.input.onFocusLost = [&] {
        SettingsWrapper::setUpperMPEPB(upperMPEPitchbendRange.getValue());
    };
    
    tabPages[0] = new TabPage(0, DeviceType::Alpha);
    tabPages[1] = new TabPage(1, DeviceType::Tau);
    tabPages[2] = new TabPage(2, DeviceType::Pico);
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    tabs.addTab("Alpha", bgColour, tabPages[0], false);
    tabs.addTab("Tau", bgColour, tabPages[1], false);
    tabs.addTab("Pico", bgColour, tabPages[2], false);
    tabs.setCurrentTabIndex(2);
    
    oscIPLabel.setText("Core IP:", juce::dontSendNotification);
    addAndMakeVisible(tabs);
    addAndMakeVisible(oscIPLabel);
    addAndMakeVisible(oscIPInput);
    addAndMakeVisible(lowerMPEVoiceCount);
    addAndMakeVisible(upperMPEVoiceCount);
    addAndMakeVisible(lowerMPEPitchbendRange);
    addAndMakeVisible(upperMPEPitchbendRange);
    
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
    upperMPEPitchbendRange.setBounds(header.removeFromRight(area.getWidth()*0.12));
    header.removeFromRight(area.getWidth()*0.02);
    lowerMPEPitchbendRange.setBounds(header.removeFromRight(area.getWidth()*0.12));
    header.removeFromRight(area.getWidth()*0.02);
    upperMPEVoiceCount.setBounds(header.removeFromRight(area.getWidth()*0.12));
    header.removeFromRight(area.getWidth()*0.02);
    lowerMPEVoiceCount.setBounds(header.removeFromRight(area.getWidth()*0.12));
    tabs.setBounds(area);
    
}

//void MainComponent::addListener(juce::ValueTree::Listener *listener) {
//}

void MainComponent::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
}
