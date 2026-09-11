#include "MainComponent.h"
#include "AppStyle.h"
#include "CalibrationDialogComponent.h"
#include "../PluginProcessor.h"
#include "../Core/SettingsWrapper.h"

namespace ecm {

namespace {

constexpr int standaloneMidi20ButtonWidth = 132;
constexpr int pluginVst3DirectButtonWidth = 164;
constexpr int calibrationButtonWidth = 38;

void updateMpeControlsEnabled(juce::Component& component, bool enabled)
{
    component.setEnabled(enabled);
}

juce::String getMidi20ButtonText(bool midi2Enabled)
{
    return midi2Enabled ? "MIDI 2.0 ON" : "MIDI 2.0 OFF";
}

juce::String getVst3DirectButtonText(bool vst3DirectEnabled)
{
    return vst3DirectEnabled ? "VST3 Direct ON" : "VST3 Direct OFF";
}

void configureModeButton(juce::TextButton& button, int radioGroupId = 0)
{
    button.setClickingTogglesState(true);
    if (radioGroupId > 0)
        button.setRadioGroupId(radioGroupId);
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
      calibrationButton(juce::CharPointer_UTF8("\xE2\x9A\x99")),
      processor(processorToUse),
      pluginState(pluginStateToUse),
      deviceManager(deviceManagerToUse) {

    isStandaloneApp_ = juce::JUCEApplicationBase::isStandaloneApp();

    pendingModeMessage.setText("This change will take effect the next time ECMapper is started", juce::dontSendNotification);
    pendingModeMessage.setJustificationType(juce::Justification::centred);
    pendingModeMessage.setColour(juce::Label::textColourId, juce::Colours::orange);
    pendingModeMessage.setVisible(false);
    addAndMakeVisible(pendingModeMessage);

    SettingsWrapper::addListener(this, pluginState.state);
    presetComboBox.setEditableText(false);
    presetComboBox.setJustificationType(juce::Justification::centredLeft);
    presetComboBox.getProperties().set("hideArrow", true);
    presetComboBox.onBrowseRequested = [this]
    {
        showPresetBrowser();
    };
    presetComboBox.onChange = [this]
    {
        if (ignorePresetComboChange_)
            return;

        handlePresetSelectionChanged();
    };
    addAndMakeVisible(presetComboBox);
    refreshPresetComboBox();
    startTimerHz(4);
    
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
    pendingMidi2Mode = midi2ModeEnabled;
    configureModeButton(midi20ModeButton);
    configureModeButton(vst3DirectModeButton);
    vst3DirectModeButton.setButtonText("VST3 Direct");
    midi20ModeButton.setTooltip("Turn MIDI 2.0 output on or off. When off, ECMapper uses the legacy MIDI/MPE path instead. Choose MIDI 2.0 when your host or device supports it. Changes take effect the next time ECMapper is started.");
    vst3DirectModeButton.setTooltip("Turn VST3 Direct output on or off. When off, ECMapper uses the legacy MIDI output path instead. Choose VST3 Direct when your host supports direct note-expression style output. Changes take effect the next time ECMapper is started.");

    midi20ModeButton.onClick = [this] {
        juce::Logger::writeToLog("MainComponent: MIDI 2.0 button clicked");
        SettingsWrapper::setMidi2Mode(midi20ModeButton.getToggleState(), this->pluginState.state);
    };
    vst3DirectModeButton.onClick = [this] {
        juce::Logger::writeToLog("MainComponent: VST3 Direct button clicked");
        SettingsWrapper::setPluginOutputMode(vst3DirectModeButton.getToggleState()
                                                 ? OutputTransportMode::Vst3Direct
                                                 : OutputTransportMode::LegacyMidi,
                                             this->pluginState.state);
    };
    addAndMakeVisible(midi20ModeButton);
    addAndMakeVisible(vst3DirectModeButton);
    pluginOutputModeIsVst3Direct_ = SettingsWrapper::getPluginOutputMode(pluginState.state) == OutputTransportMode::Vst3Direct;
    refreshTransportModeControls();
    
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

    calibrationButton.setTooltip("Open calibration settings");
    calibrationButton.getProperties().set("largeIcon", true);
    calibrationButton.setColour(juce::TextButton::buttonColourId, Style::surfaceRaised());
    calibrationButton.setColour(juce::TextButton::buttonOnColourId, Style::accentStrong());
    calibrationButton.setColour(juce::TextButton::textColourOffId, Style::text());
    calibrationButton.setColour(juce::TextButton::textColourOnId, Style::background());
    calibrationButton.onClick = [this] { showCalibrationDialog(); };
    addAndMakeVisible(calibrationButton);

    currentTabIndex = juce::jlimit(0, 3, SettingsWrapper::getCurrentTabIndex(pluginState.state));
    selectTab(currentTabIndex);

    addAndMakeVisible(lowerMPEVoiceCount);
    addAndMakeVisible(upperMPEVoiceCount);
    addAndMakeVisible(lowerMPEPitchbendRange);
    addAndMakeVisible(upperMPEPitchbendRange);
}

MainComponent::~MainComponent() {
    stopTimer();
    pluginState.state.removeListener(this);
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(Style::background());

    g.setColour(Style::border());
    g.drawRect(getLocalBounds(), 1);

    auto header = getLocalBounds().removeFromTop(106);
    g.setColour(Style::background().interpolatedWith(Style::surface(), 0.24f));
    g.fillRect(header);

    g.setColour(Style::accent().withAlpha(0.70f));
    g.fillRect(header.removeFromTop(3));
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(106); // Increased header height from 86 to 106
    header.reduce(10, 8);

    auto topRow = header.removeFromTop(30);
    pendingModeMessage.setBounds(header.removeFromTop(20).reduced(10, 0));
    auto bottomRow = header.removeFromTop(34);

    auto presetWidth = juce::jlimit(160, 360, topRow.getWidth() / 4);
    presetComboBox.setBounds(topRow.removeFromLeft(presetWidth).withHeight(28));
    topRow.removeFromLeft(8);

    auto controlArea = topRow;
    controlArea.removeFromLeft(12);
    controlArea.removeFromRight(12);
    auto controlWidth = 120;
    auto modeWidth = isStandaloneApp_ ? standaloneMidi20ButtonWidth : pluginVst3DirectButtonWidth;
    auto modeGap = 2;

    upperMPEPitchbendRange.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    lowerMPEPitchbendRange.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    upperMPEVoiceCount.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(8);
    lowerMPEVoiceCount.setBounds(controlArea.removeFromRight(controlWidth).withHeight(28));
    controlArea.removeFromRight(4);
    if (isStandaloneApp_) {
        midi20ModeButton.setBounds(controlArea.removeFromRight(modeWidth).withSizeKeepingCentre(modeWidth, 24));
    } else {
        vst3DirectModeButton.setBounds(controlArea.removeFromRight(modeWidth).withSizeKeepingCentre(modeWidth, 24));
    }


    auto tabArea = bottomRow.reduced(0, 1);
    calibrationButton.setBounds(tabArea.removeFromRight(calibrationButtonWidth).reduced(4, 0));
    tabArea.removeFromRight(4);
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

void MainComponent::refreshPresetComboBox()
{
    for (int slot = 1; slot <= ECMapperAudioProcessor::numPresetSlots; ++slot) {
        auto text = processor.getPresetSlotDisplayName(slot);
        if (presetComboBox.indexOfItemId(slot) < 0)
            presetComboBox.addItem(text, slot);
        else
            presetComboBox.changeItemText(slot, text);
    }

    auto selectedSlot = processor.getCurrentPresetSlot();
    if (presetComboBox.getSelectedId() != selectedSlot) {
        const juce::ScopedValueSetter<bool> guard(ignorePresetComboChange_, true);
        presetComboBox.setSelectedId(selectedSlot, juce::dontSendNotification);
    }
}

void MainComponent::showPresetBrowser()
{
    juce::DialogWindow::LaunchOptions options;
    auto* browser = new PresetBrowserComponent(processor);
    browser->setLookAndFeel(&getLookAndFeel());
    options.content.setOwned(browser);
    options.dialogTitle = "Presets";
    options.dialogBackgroundColour = Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = this;
    
    if (auto* dw = options.launchAsync())
        dw->setLookAndFeel(&getLookAndFeel());
}

void MainComponent::showCalibrationDialog()
{
    juce::DialogWindow::LaunchOptions options;
    auto* dialog = new CalibrationDialogComponent(pluginState.state);
    dialog->setLookAndFeel(&getLookAndFeel());
    options.content.setOwned(dialog);
    options.dialogTitle = "Calibration";
    options.dialogBackgroundColour = Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;

    if (auto* dw = options.launchAsync())
        dw->setLookAndFeel(&getLookAndFeel());
}

void MainComponent::refreshFromState()
{
    if (!lowerMPEVoiceCount.input.hasKeyboardFocus(true))
        lowerMPEVoiceCount.setValue(SettingsWrapper::getLowerMPEVoiceCount(pluginState.state));

    if (!upperMPEVoiceCount.input.hasKeyboardFocus(true))
        upperMPEVoiceCount.setValue(SettingsWrapper::getUpperMPEVoiceCount(pluginState.state));

    if (!lowerMPEPitchbendRange.input.hasKeyboardFocus(true))
        lowerMPEPitchbendRange.setValue(SettingsWrapper::getLowerMPEPB(pluginState.state));

    if (!upperMPEPitchbendRange.input.hasKeyboardFocus(true))
        upperMPEPitchbendRange.setValue(SettingsWrapper::getUpperMPEPB(pluginState.state));

    pendingMidi2Mode = SettingsWrapper::getMidi2Mode(pluginState.state);
    pluginOutputModeIsVst3Direct_ = SettingsWrapper::getPluginOutputMode(pluginState.state) == OutputTransportMode::Vst3Direct;
    midi2ModeChanged = (pendingMidi2Mode != midi2ModeEnabled);
    pendingModeMessage.setVisible(midi2ModeChanged);

    refreshTransportModeControls();
}

void MainComponent::refreshTransportModeControls()
{
    updateStandaloneMidi2Button();
    updatePluginVst3DirectButton();
    vst3DirectModeButton.setVisible(!isStandaloneApp_);
    midi20ModeButton.setVisible(isStandaloneApp_);
    updateMpeControlsEnabled(lowerMPEVoiceCount, !pendingMidi2Mode);
    updateMpeControlsEnabled(upperMPEVoiceCount, !pendingMidi2Mode);
    updateMpeControlsEnabled(lowerMPEPitchbendRange, !pendingMidi2Mode);
    updateMpeControlsEnabled(upperMPEPitchbendRange, !pendingMidi2Mode);
}

void MainComponent::updateStandaloneMidi2Button()
{
    midi20ModeButton.setToggleState(pendingMidi2Mode, juce::dontSendNotification);
    midi20ModeButton.setButtonText(getMidi20ButtonText(pendingMidi2Mode));
    midi20ModeButton.setColour(juce::TextButton::buttonColourId, Style::danger());
    midi20ModeButton.setColour(juce::TextButton::buttonOnColourId, Style::accentStrong());
    midi20ModeButton.setColour(juce::TextButton::textColourOffId, Style::text());
    midi20ModeButton.setColour(juce::TextButton::textColourOnId, Style::background());
}

void MainComponent::updatePluginVst3DirectButton()
{
    vst3DirectModeButton.setToggleState(pluginOutputModeIsVst3Direct_, juce::dontSendNotification);
    vst3DirectModeButton.setButtonText(getVst3DirectButtonText(pluginOutputModeIsVst3Direct_));
    vst3DirectModeButton.setColour(juce::TextButton::buttonColourId, Style::danger());
    vst3DirectModeButton.setColour(juce::TextButton::buttonOnColourId, Style::accentStrong());
    vst3DirectModeButton.setColour(juce::TextButton::textColourOffId, Style::text());
    vst3DirectModeButton.setColour(juce::TextButton::textColourOnId, Style::background());
}

void MainComponent::timerCallback()
{
    refreshPresetComboBox();
    refreshFromState();
}

void MainComponent::handlePresetSelectionChanged()
{
    auto selectedSlot = presetComboBox.getSelectedId();
    if (selectedSlot >= 1 && selectedSlot <= ECMapperAudioProcessor::numPresetSlots) {
        processor.loadPresetSlot(selectedSlot);
        refreshPresetComboBox();
        refreshFromState();
        repaint();
    }
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree& vTree, const juce::Identifier& property) {
    juce::ignoreUnused(vTree);
    juce::Logger::writeToLog("MainComponent: Property changed: " + property.toString());

    if (property == SettingsWrapper::id_midi2Mode || property == SettingsWrapper::id_pluginOutputMode)
    {
        pendingMidi2Mode = SettingsWrapper::getMidi2Mode(pluginState.state);
        pluginOutputModeIsVst3Direct_ = SettingsWrapper::getPluginOutputMode(pluginState.state) == OutputTransportMode::Vst3Direct;
        midi2ModeChanged = (pendingMidi2Mode != midi2ModeEnabled);
        pendingModeMessage.setVisible(midi2ModeChanged);

        refreshTransportModeControls();
        repaint();
    }
}

} // namespace ecm
