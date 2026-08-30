#include "MainComponent.h"

namespace ecm {

MainComponent::MainComponent(juce::AudioProcessorValueTreeState& pluginStateToUse, HardwareService& hardwareService, juce::AudioDeviceManager* deviceManagerToUse)
    : lowerMPEVoiceCount("Lower MPE voices:", 2, 0, 15, true), 
      upperMPEVoiceCount("Upper MPE voices:", 2, 0, 15, true),  
      lowerMPEPitchbendRange("Lower MPE pb:", 2, 0, 96, true), 
      upperMPEPitchbendRange("Upper MPE pb:", 2, 0, 96, true),
      pluginState(pluginStateToUse),
      deviceManager(deviceManagerToUse) {
    
    SettingsWrapper::addListener(this, pluginState.state);
    
    lowerMPEVoiceCount.setValue(SettingsWrapper::getLowerMPEVoiceCount(pluginState.state));
    lowerMPEVoiceCount.input.onFocusLost = [this] {
        SettingsWrapper::setLowerMPEVoiceCount(lowerMPEVoiceCount.getValue(), this->pluginState.state);
    };

    upperMPEVoiceCount.setValue(SettingsWrapper::getUpperMPEVoiceCount(pluginState.state));
    upperMPEVoiceCount.input.onFocusLost = [this] {
        SettingsWrapper::setUpperMPEVoiceCount(upperMPEVoiceCount.getValue(), this->pluginState.state);
    };
    
    lowerMPEPitchbendRange.setValue(SettingsWrapper::getLowerMPEPB(pluginState.state));
    lowerMPEPitchbendRange.input.onFocusLost = [this] {
        SettingsWrapper::setLowerMPEPB(lowerMPEPitchbendRange.getValue(), this->pluginState.state);
    };

    upperMPEPitchbendRange.setValue(SettingsWrapper::getUpperMPEPB(pluginState.state));
    upperMPEPitchbendRange.input.onFocusLost = [this] {
        SettingsWrapper::setUpperMPEPB(upperMPEPitchbendRange.getValue(), this->pluginState.state);
    };
    
    auto bgColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    
    corePage = std::make_unique<CorePage>(hardwareService, pluginState.state);
    alphaPage = std::make_unique<TabPage>(0, InstrumentType::Alpha, pluginState);
    tauPage = std::make_unique<TabPage>(1, InstrumentType::Tau, pluginState);
    picoPage = std::make_unique<TabPage>(2, InstrumentType::Pico, pluginState);
    
    tabs.addTab("Communication", bgColour, corePage.get(), false);
    tabs.addTab("Alpha", bgColour, alphaPage.get(), false);
    tabs.addTab("Tau", bgColour, tauPage.get(), false);
    tabs.addTab("Pico", bgColour, picoPage.get(), false);
    
    tabs.setCurrentTabIndex(SettingsWrapper::getCurrentTabIndex(pluginState.state));
    tabs.onTabChanged = [this](int index) {
        SettingsWrapper::setCurrentTabIndex(index, this->pluginState.state);
    };
    
    if (deviceManager != nullptr) {
        audioSettingsButton.setButtonText("Audio/MIDI Settings");
        audioSettingsButton.onClick = [this] {
            auto* selector = new juce::AudioDeviceSelectorComponent(*this->deviceManager,
                                                                  0, 256, 0, 256, true, true, true, false);
            selector->setSize(500, 450);
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(selector);
            options.dialogTitle = "Audio/MIDI Settings";
            options.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = false;
            options.launchAsync();
        };
        addAndMakeVisible(audioSettingsButton);
    }

    addAndMakeVisible(tabs);
    addAndMakeVisible(lowerMPEVoiceCount);
    addAndMakeVisible(upperMPEVoiceCount);
    addAndMakeVisible(lowerMPEPitchbendRange);
    addAndMakeVisible(upperMPEPitchbendRange);
}

MainComponent::~MainComponent() {
    pluginState.state.removeListener(this);
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(40);
    header.reduce(10, 5);
    
    if (deviceManager != nullptr) {
        audioSettingsButton.setBounds(header.removeFromLeft(150));
    }
    
    header.removeFromRight(10);
    upperMPEPitchbendRange.setBounds(header.removeFromRight(80));
    header.removeFromRight(10);
    lowerMPEPitchbendRange.setBounds(header.removeFromRight(80));
    header.removeFromRight(10);
    upperMPEVoiceCount.setBounds(header.removeFromRight(80));
    header.removeFromRight(10);
    lowerMPEVoiceCount.setBounds(header.removeFromRight(80));
    
    tabs.setBounds(area);
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    juce::ignoreUnused(vTree, property);
}

} // namespace ecm
