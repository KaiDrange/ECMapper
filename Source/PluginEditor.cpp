#include "PluginProcessor.h"
#include "PluginEditor.h"

ECMapperAudioProcessorEditor::ECMapperAudioProcessorEditor(ECMapperAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    
    mainComponent = std::make_unique<ecm::MainComponent>(p.state, p.getHardwareService());
    addAndMakeVisible(mainComponent.get());
    
    setResizable(true, true);
    setResizeLimits(800, 600, 4096, 4096);
    setSize(1000, 700);
}

ECMapperAudioProcessorEditor::~ECMapperAudioProcessorEditor() {
}

void ECMapperAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ECMapperAudioProcessorEditor::resized() {
    mainComponent->setBounds(getLocalBounds());
}
