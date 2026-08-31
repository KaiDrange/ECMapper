#include "MainComponent.h"
#include "AppStyle.h"

namespace ecm {

MainComponent::MainComponent(juce::AudioProcessorValueTreeState& pluginStateToUse, HardwareService& hardwareService, juce::AudioDeviceManager* deviceManagerToUse)
    : lowerMPEVoiceCount("Lower MPE voices:", 2, 0, 15, true), 
      upperMPEVoiceCount("Upper MPE voices:", 2, 0, 15, true),  
      lowerMPEPitchbendRange("Lower MPE pb:", 2, 0, 96, true), 
      upperMPEPitchbendRange("Upper MPE pb:", 2, 0, 96, true),
      communicationTabButton("Communication"),
      alphaTabButton("Alpha"),
      tauTabButton("Tau"),
      picoTabButton("Pico"),
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
    
    corePage = std::make_unique<CorePage>(hardwareService, pluginState.state);
    alphaPage = std::make_unique<TabPage>(0, InstrumentType::Alpha, pluginState);
    tauPage = std::make_unique<TabPage>(1, InstrumentType::Tau, pluginState);
    picoPage = std::make_unique<TabPage>(2, InstrumentType::Pico, pluginState);

    addAndMakeVisible(corePage.get());
    addAndMakeVisible(alphaPage.get());
    addAndMakeVisible(tauPage.get());
    addAndMakeVisible(picoPage.get());
    
    auto configureTab = [this](juce::TextButton& button, const juce::String& text, juce::Colour colour, int index)
    {
        button.setButtonText(text);
        button.setClickingTogglesState(true);
        button.setRadioGroupId(1);
        button.setColour(juce::TextButton::buttonColourId, colour);
        button.setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.25f));
        button.setColour(juce::TextButton::textColourOffId, Style::text());
        button.setColour(juce::TextButton::textColourOnId, Style::background());
        button.onClick = [this, index] { selectTab(index); };
        addAndMakeVisible(button);
    };

    configureTab(communicationTabButton, "Communication", juce::Colour(0xff4d79a6), 0);
    configureTab(alphaTabButton, "Alpha", juce::Colour(0xff3f8aa8), 1);
    configureTab(tauTabButton, "Tau", juce::Colour(0xff5f7aa8), 2);
    configureTab(picoTabButton, "Pico", juce::Colour(0xff4e8f84), 3);

    currentTabIndex = juce::jlimit(0, 3, SettingsWrapper::getCurrentTabIndex(pluginState.state));
    selectTab(currentTabIndex);
    
    if (deviceManager != nullptr) {
        audioSettingsButton.setButtonText("Audio/MIDI Settings");
        audioSettingsButton.onClick = [this] {
            auto* selector = new juce::AudioDeviceSelectorComponent(*this->deviceManager,
                                                                  0, 256, 0, 256, true, true, true, false);
            selector->setSize(500, 450);
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(selector);
            options.dialogTitle = "Audio/MIDI Settings";
            options.dialogBackgroundColour = Style::background();
            options.escapeKeyTriggersCloseButton = true;
            options.useNativeTitleBar = true;
            options.resizable = false;
            options.launchAsync();
        };
        addAndMakeVisible(audioSettingsButton);
    }

    addAndMakeVisible(lowerMPEVoiceCount);
    addAndMakeVisible(upperMPEVoiceCount);
    addAndMakeVisible(lowerMPEPitchbendRange);
    addAndMakeVisible(upperMPEPitchbendRange);
}

MainComponent::~MainComponent() {
    pluginState.state.removeListener(this);
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(Style::background());

    g.setColour(Style::border());
    g.drawRect(getLocalBounds(), 1);

    auto header = getLocalBounds().removeFromTop(86);
    g.setColour(Style::background().interpolatedWith(Style::surface(), 0.24f));
    g.fillRect(header);

    g.setColour(Style::accent().withAlpha(0.70f));
    g.fillRect(header.removeFromTop(3));
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(86);
    header.reduce(10, 8);

    auto topRow = header.removeFromTop(30);
    auto bottomRow = header.removeFromTop(34);

    if (deviceManager != nullptr) {
        audioSettingsButton.setBounds(topRow.removeFromLeft(158).withSizeKeepingCentre(158, 24));
    }

    auto controlArea = topRow;
    controlArea.removeFromLeft(12);
    controlArea.removeFromRight(12);
    auto controlWidth = 120;
    upperMPEPitchbendRange.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    lowerMPEPitchbendRange.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    upperMPEVoiceCount.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    lowerMPEVoiceCount.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));

    auto tabArea = bottomRow.reduced(0, 1);
    auto tabWidth = tabArea.getWidth() / 4;
    communicationTabButton.setBounds(tabArea.removeFromLeft(tabWidth).reduced(0, 0));
    alphaTabButton.setBounds(tabArea.removeFromLeft(tabWidth).reduced(4, 0));
    tauTabButton.setBounds(tabArea.removeFromLeft(tabWidth).reduced(4, 0));
    picoTabButton.setBounds(tabArea.reduced(4, 0));
    
    auto contentArea = area;
    corePage->setVisible(currentTabIndex == 0);
    alphaPage->setVisible(currentTabIndex == 1);
    tauPage->setVisible(currentTabIndex == 2);
    picoPage->setVisible(currentTabIndex == 3);

    corePage->setBounds(contentArea);
    alphaPage->setBounds(contentArea);
    tauPage->setBounds(contentArea);
    picoPage->setBounds(contentArea);
}

void MainComponent::selectTab(int index)
{
    index = juce::jlimit(0, 3, index);
    currentTabIndex = index;

    communicationTabButton.setToggleState(index == 0, juce::dontSendNotification);
    alphaTabButton.setToggleState(index == 1, juce::dontSendNotification);
    tauTabButton.setToggleState(index == 2, juce::dontSendNotification);
    picoTabButton.setToggleState(index == 3, juce::dontSendNotification);

    corePage->setVisible(index == 0);
    alphaPage->setVisible(index == 1);
    tauPage->setVisible(index == 2);
    picoPage->setVisible(index == 3);

    SettingsWrapper::setCurrentTabIndex(index, this->pluginState.state);
    resized();
    repaint();
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    juce::ignoreUnused(vTree, property);
}

} // namespace ecm
