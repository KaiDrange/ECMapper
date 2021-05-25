#include "PluginProcessor.h"
#include "PluginEditor.h"

EigenharpMapperAudioProcessorEditor::EigenharpMapperAudioProcessorEditor (EigenharpMapperAudioProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p) {
    mainComponent = new MainComponent(p.pluginState.state);
    setSize (800, 600);
    addAndMakeVisible(mainComponent);
    mainComponent->setBounds(getLocalBounds());
}

EigenharpMapperAudioProcessorEditor::~EigenharpMapperAudioProcessorEditor() {
    delete mainComponent;
}

void EigenharpMapperAudioProcessorEditor::resized() {
    mainComponent->setBounds(getLocalBounds());
}

Layout* EigenharpMapperAudioProcessorEditor::getLayout(DeviceType deviceType) {
    return mainComponent->getLayout(deviceType);
}

void EigenharpMapperAudioProcessorEditor::recreateMainComponent() {
    delete mainComponent;
    mainComponent = new MainComponent(audioProcessor.pluginState.state);
    addAndMakeVisible(mainComponent);
    mainComponent->setBounds(getLocalBounds());
    mainComponent->addListener(&audioProcessor.layoutChangeHandler);
}
