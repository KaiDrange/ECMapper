#include "TabPage.h"
#include "AppStyle.h"
#include "../Core/LayoutWrapper.h"

namespace ecm {

TabPage::TabPage(int tabIndex, InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState) 
    : deviceType(deviceType), 
      tabIndex_(tabIndex),
      keyboard(keyboardState, juce::MidiKeyboardComponent::Orientation::verticalKeyboardFacingRight),
      pluginState(pluginState) {
    
    juce::ignoreUnused(tabIndex);
    
    layoutPanel = std::make_unique<LayoutComponent>(deviceType, 0.4f, 1.0f, pluginState);
    addAndMakeVisible(layoutPanel.get());
    
    for (int i = 0; i < 3; i++) {
        zonePanels[i] = std::make_unique<ZonePanelComponent>(deviceType, static_cast<Zone>(i + 1), 1.0f, 1.0f / 3.0f, pluginState);
        addAndMakeVisible(zonePanels[i].get());
    }
    
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
}

TabPage::~TabPage() {
    keyboardState.removeListener(layoutPanel.get());
    keyboardState.removeListener(&layoutPanel->chordSectionComponent);
}

void TabPage::paint(juce::Graphics& g) {
    g.fillAll(Style::background());

    g.setColour(Style::tabColour(tabIndex_).withAlpha(0.80f));
    g.fillRect(0, 0, getWidth(), 3);
}

void TabPage::resized() {
    auto area = getLocalBounds();
    area.reduce(5, 5);
    
    keyboard.setBounds(area.removeFromLeft(area.getWidth() * 0.1f));
    area.removeFromLeft(area.getWidth() * 0.01f);
    
    auto btnArea = area.removeFromTop(static_cast<int>(area.getHeight() * 0.05f));
    loadMappingButton.setBounds(btnArea.removeFromLeft(area.getWidth() * 0.1f));
    saveMappingButton.setBounds(btnArea.removeFromLeft(area.getWidth() * 0.1f));
    clearMappingButton.setBounds(btnArea.removeFromLeft(area.getWidth() * 0.1f));

    layoutPanel->setBounds(area.removeFromLeft(static_cast<int>(area.getWidth() * 0.4f)));
    
    auto zoneArea = area.removeFromRight(static_cast<int>(area.getWidth() * 0.98f));
    for (const auto & zonePanel : zonePanels) {
        zonePanel->setBounds(zoneArea.removeFromTop(area.getHeight() / 3));
    }
}

} // namespace ecm
