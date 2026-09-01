#include "TabPage.h"
#include "AppStyle.h"
#include "../PluginProcessor.h"
#include "../Core/LayoutWrapper.h"

namespace ecm {

namespace {

void configureViewButton(juce::TextButton& button)
{
    button.setClickingTogglesState(true);
    button.setRadioGroupId(3);
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff395060));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2bb6df));
    button.setColour(juce::TextButton::textColourOffId, Style::text());
    button.setColour(juce::TextButton::textColourOnId, Style::background());
}

}

TabPage::TabPage(int tabIndex, InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState, ECMapperAudioProcessor& processor) 
    : deviceType(deviceType), 
      tabIndex_(tabIndex),
      keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight),
      processor(processor),
      pluginState(pluginState) {

    layoutPanel = std::make_unique<LayoutComponent>(deviceType, 0.4f, 1.0f, pluginState);
    addAndMakeVisible(layoutPanel.get());

    expressionCurvesComponent = std::make_unique<ExpressionCurvesComponent>(deviceType, pluginState);
    addAndMakeVisible(expressionCurvesComponent.get());
    
    for (int i = 0; i < 3; i++) {
        zonePanels[i] = std::make_unique<ZonePanelComponent>(deviceType, static_cast<Zone>(i + 1), 1.0f, 1.0f / 3.0f, pluginState);
        addAndMakeVisible(zonePanels[i].get());
    }

    configureViewButton(curvesViewButton);
    configureViewButton(zonesViewButton);
    curvesViewButton.setConnectedEdges(juce::Button::ConnectedOnRight);
    zonesViewButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    curvesViewButton.setButtonText("Curves");
    zonesViewButton.setButtonText("Zones");
    curvesViewButton.onClick = [this] {
        if (curvesViewButton.getToggleState())
            setRightPanelView(RightPanelView::Curves);
    };
    zonesViewButton.onClick = [this] {
        if (zonesViewButton.getToggleState())
            setRightPanelView(RightPanelView::Zones);
    };
    addAndMakeVisible(curvesViewButton);
    addAndMakeVisible(zonesViewButton);
    curvesViewButton.setToggleState(true, juce::dontSendNotification);
    zonesViewButton.setToggleState(false, juce::dontSendNotification);

    addAndMakeVisible(keyboard);
    keyboardState.addListener(layoutPanel.get());
    keyboardState.addListener(&layoutPanel->chordSectionComponent);

    addAndMakeVisible(saveMappingButton);
    saveMappingButton.onClick = [this] {
        // TODO: Port FileUtil mapping save
    };
    
    addAndMakeVisible(loadMappingButton);
    loadMappingButton.onClick = [this] {
        // TODO: Port FileUtil mapping load
    };
    
    addAndMakeVisible(clearMappingButton);
    clearMappingButton.onClick = [this] {
        layoutPanel->deselectAllKeys();
        auto oldTree = LayoutWrapper::getLayoutTree(this->deviceType, this->pluginState.state);
        // Reset properties to default
    };
    
    addKeyListener(layoutPanel.get());

    setRightPanelView(rightPanelView);
    setActive(isVisible());
}

TabPage::~TabPage() {
    stopTimer();
    keyboardState.removeListener(layoutPanel.get());
    keyboardState.removeListener(&layoutPanel->chordSectionComponent);
}

void TabPage::paint(juce::Graphics& g) {
    g.fillAll(Style::background());

    g.setColour(Style::tabColour(tabIndex_).withAlpha(0.80f));
    g.fillRect(0, 0, getWidth(), 3);
}

void TabPage::setRightPanelView(RightPanelView view)
{
    rightPanelView = view;
    auto showCurves = rightPanelView == RightPanelView::Curves;

    curvesViewButton.setToggleState(showCurves, juce::dontSendNotification);
    zonesViewButton.setToggleState(!showCurves, juce::dontSendNotification);
    expressionCurvesComponent->setVisible(showCurves);
    for (auto& zonePanel : zonePanels)
        zonePanel->setVisible(!showCurves);

    resized();
    repaint();
}

void TabPage::setActive(bool active)
{
    processor.clearKeyboardSelectionMessages();

    if (active) {
        keyboardState.reset();
        startTimerHz(30);
    } else {
        stopTimer();
        keyboardState.reset();
    }
}

void TabPage::resized() {
    auto area = getLocalBounds();
    area.reduce(5, 5);
    
    keyboard.setBounds(area.removeFromLeft(static_cast<int>(area.getWidth() * 0.1f)));
    area.removeFromLeft(static_cast<int>(area.getWidth() * 0.01f));
    
    auto btnArea = area.removeFromTop(static_cast<int>(area.getHeight() * 0.05f));
    loadMappingButton.setBounds(btnArea.removeFromLeft(static_cast<int>(area.getWidth() * 0.1f)));
    saveMappingButton.setBounds(btnArea.removeFromLeft(static_cast<int>(area.getWidth() * 0.1f)));
    clearMappingButton.setBounds(btnArea.removeFromLeft(static_cast<int>(area.getWidth() * 0.1f)));

    auto layoutWidth = static_cast<int>(area.getWidth() * 0.38f);
    layoutPanel->setBounds(area.removeFromLeft(layoutWidth));
    area.removeFromLeft(10);
    
    auto rightContent = area;

    auto viewButtons = rightContent.removeFromTop(28);
    viewButtons.removeFromRight(2);
    curvesViewButton.setBounds(viewButtons.removeFromLeft(viewButtons.getWidth() / 2).reduced(0, 0));
    viewButtons.removeFromLeft(4);
    zonesViewButton.setBounds(viewButtons);

    rightContent.removeFromTop(8);

    if (rightPanelView == RightPanelView::Curves) {
        expressionCurvesComponent->setBounds(rightContent);
    } else {
        auto zoneArea = rightContent;
        auto zoneHeight = zoneArea.getHeight() / 3;
        auto remainder = zoneArea.getHeight() % 3;
        for (const auto& zonePanel : zonePanels) {
            auto sliceHeight = zoneHeight + (remainder > 0 ? 1 : 0);
            if (remainder > 0)
                --remainder;
            zonePanel->setBounds(zoneArea.removeFromTop(sliceHeight));
        }
    }
}

void TabPage::timerCallback() {
    std::vector<juce::MidiMessage> messages;
    processor.drainKeyboardSelectionMessages(messages);

    for (const auto& message : messages) {
        if (message.isNoteOn() || message.isNoteOff())
            keyboardState.processNextMidiEvent(message);
    }
}

} // namespace ecm
