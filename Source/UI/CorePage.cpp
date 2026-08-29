#include "CorePage.h"

namespace ecm {

CorePage::CorePage(HardwareService& hardwareService) : hardwareService_(hardwareService) {
    ledGreen = juce::ImageFileFormat::loadFrom(BinaryData::GreenLight_png, BinaryData::GreenLight_pngSize);
    ledOff = juce::ImageFileFormat::loadFrom(BinaryData::DarkLight_png, BinaryData::DarkLight_pngSize);
    startTimer(200);
}

void CorePage::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    auto area = getLocalBounds().reduced(20);
    auto ledArea = area.removeFromTop(100);
    
    juce::StringArray names = { "Pico", "Tau", "Alpha", "Service" };
    bool statuses[4] = { false, false, false, hardwareService_.isServiceRunning() };
    
    // For now, let's just show service status. 
    // In a real app we'd get device connection status from HardwareService.
    
    float ledSize = 40.0f;
    float spacing = (ledArea.getWidth() - (4 * ledSize)) / 5;
    
    for (int i = 0; i < 4; i++) {
        float x = spacing + i * (ledSize + spacing);
        float y = ledArea.getY() + 10;
        
        g.drawImage(statuses[i] ? ledGreen : ledOff, x, y, ledSize, ledSize, 0, 0, ledGreen.getWidth(), ledGreen.getHeight());
        
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawFittedText(names[i], static_cast<int>(x), static_cast<int>(y + ledSize + 5), static_cast<int>(ledSize), 20, juce::Justification::centred, false);
    }

    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
    
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("EigenCore Status", area.removeFromTop(30), juce::Justification::centred, false);
}

void CorePage::resized() {
}

} // namespace ecm
