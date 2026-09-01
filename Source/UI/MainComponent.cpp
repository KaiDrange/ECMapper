#include "MainComponent.h"
#include "AppStyle.h"
#include "../Core/SettingsWrapper.h"

namespace ecm {

namespace {

void updateMpeControlsEnabled(juce::Component& component, bool enabled)
{
    component.setEnabled(enabled);
}

void configureModeButton(juce::TextButton& button)
{
    button.setClickingTogglesState(true);
    button.setRadioGroupId(2);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff395060));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2bb6df));
    button.setColour(juce::TextButton::textColourOffId, Style::text());
    button.setColour(juce::TextButton::textColourOnId, Style::background());
}

}

MainComponent::MainComponent(juce::AudioProcessorValueTreeState& pluginStateToUse, HardwareService& hardwareService, ECMapperAudioProcessor& processorToUse, juce::AudioDeviceManager* deviceManagerToUse)
    : lowerMPEVoiceCount("Lower MPE voices:", 2, 0, 15, true), 
      upperMPEVoiceCount("Upper MPE voices:", 2, 0, 15, true),  
      lowerMPEPitchbendRange("Lower MPE pb:", 2, 0, 96, true), 
      upperMPEPitchbendRange("Upper MPE pb:", 2, 0, 96, true),
      communicationTabButton("Communication"),
      alphaTabButton("Alpha"),
      tauTabButton("Tau"),
      picoTabButton("Pico"),
      processor(processorToUse),
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

    midi2ModeEnabled = SettingsWrapper::getMidi2Mode(pluginState.state);
    configureModeButton(mpeModeButton);
    configureModeButton(midi20ModeButton);
    mpeModeButton.setConnectedEdges(juce::Button::ConnectedOnRight);
    midi20ModeButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    mpeModeButton.setButtonText("MPE");
    midi20ModeButton.setButtonText("MIDI 2.0");
    mpeModeButton.onClick = [this] {
        if (!mpeModeButton.getToggleState())
            return;
        midi2ModeEnabled = false;
        SettingsWrapper::setMidi2Mode(midi2ModeEnabled, this->pluginState.state);
        updateMpeControlsEnabled(lowerMPEVoiceCount, true);
        updateMpeControlsEnabled(upperMPEVoiceCount, true);
        updateMpeControlsEnabled(lowerMPEPitchbendRange, true);
        updateMpeControlsEnabled(upperMPEPitchbendRange, true);
        repaint();
    };
    midi20ModeButton.onClick = [this] {
        if (!midi20ModeButton.getToggleState())
            return;
        midi2ModeEnabled = true;
        SettingsWrapper::setMidi2Mode(midi2ModeEnabled, this->pluginState.state);
        updateMpeControlsEnabled(lowerMPEVoiceCount, false);
        updateMpeControlsEnabled(upperMPEVoiceCount, false);
        updateMpeControlsEnabled(lowerMPEPitchbendRange, false);
        updateMpeControlsEnabled(upperMPEPitchbendRange, false);
        repaint();
    };
    addAndMakeVisible(mpeModeButton);
    addAndMakeVisible(midi20ModeButton);
    mpeModeButton.setToggleState(!midi2ModeEnabled, juce::dontSendNotification);
    midi20ModeButton.setToggleState(midi2ModeEnabled, juce::dontSendNotification);
    updateMpeControlsEnabled(lowerMPEVoiceCount, !midi2ModeEnabled);
    updateMpeControlsEnabled(upperMPEVoiceCount, !midi2ModeEnabled);
    updateMpeControlsEnabled(lowerMPEPitchbendRange, !midi2ModeEnabled);
    updateMpeControlsEnabled(upperMPEPitchbendRange, !midi2ModeEnabled);
    
    corePage = std::make_unique<CorePage>(hardwareService, pluginState.state);
    alphaPage = std::make_unique<TabPage>(0, InstrumentType::Alpha, pluginState, processor);
    tauPage = std::make_unique<TabPage>(1, InstrumentType::Tau, pluginState, processor);
    picoPage = std::make_unique<TabPage>(2, InstrumentType::Pico, pluginState, processor);

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

    configureTab(communicationTabButton, "Communication", Style::tabColour(0), 0);
    configureTab(alphaTabButton, "Alpha", Style::tabColour(1), 1);
    configureTab(tauTabButton, "Tau", Style::tabColour(2), 2);
    configureTab(picoTabButton, "Pico", Style::tabColour(3), 3);

    currentTabIndex = juce::jlimit(0, 3, SettingsWrapper::getCurrentTabIndex(pluginState.state));
    selectTab(currentTabIndex);

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

    auto controlArea = topRow;
    controlArea.removeFromLeft(12);
    controlArea.removeFromRight(12);
    auto controlWidth = 120;
    auto modeWidth = 72;
    auto modeGap = 2;

    upperMPEPitchbendRange.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    lowerMPEPitchbendRange.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    upperMPEVoiceCount.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    lowerMPEVoiceCount.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(4);
    midi20ModeButton.setBounds(controlArea.removeFromRight(modeWidth).withSizeKeepingCentre(modeWidth, 24));
    controlArea.removeFromRight(modeGap);
    mpeModeButton.setBounds(controlArea.removeFromRight(modeWidth).withSizeKeepingCentre(modeWidth, 24));

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

    alphaPage->setActive(index == 1);
    tauPage->setActive(index == 2);
    picoPage->setActive(index == 3);

    SettingsWrapper::setCurrentTabIndex(index, this->pluginState.state);
    resized();
    repaint();
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    juce::ignoreUnused(vTree);

    if (property == SettingsWrapper::id_midi2Mode)
    {
        midi2ModeEnabled = SettingsWrapper::getMidi2Mode(pluginState.state);
        mpeModeButton.setToggleState(!midi2ModeEnabled, juce::dontSendNotification);
        midi20ModeButton.setToggleState(midi2ModeEnabled, juce::dontSendNotification);
        updateMpeControlsEnabled(lowerMPEVoiceCount, !midi2ModeEnabled);
        updateMpeControlsEnabled(upperMPEVoiceCount, !midi2ModeEnabled);
        updateMpeControlsEnabled(lowerMPEPitchbendRange, !midi2ModeEnabled);
        updateMpeControlsEnabled(upperMPEPitchbendRange, !midi2ModeEnabled);
        repaint();
    }
}

} // namespace ecm
