#include "PluginProcessor.h"
#include "PluginEditor.h"

ECMapperAudioProcessorEditor::ECMapperAudioProcessorEditor(ECMapperAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    
    mainComponent = std::make_unique<ecm::MainComponent>(p.state, p.getHardwareService(), p, p.getDeviceManager());
    addAndMakeVisible(mainComponent.get());
    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    setResizable(true, true);
    setResizeLimits(800, 600, 4096, 4096);
    setSize(1000, 700);
}

ECMapperAudioProcessorEditor::~ECMapperAudioProcessorEditor() {
    mainComponent.reset();
    setLookAndFeel(nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void ECMapperAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(ecm::Style::background());
}

void ECMapperAudioProcessorEditor::resized() {
    mainComponent->setBounds(getLocalBounds());
}
