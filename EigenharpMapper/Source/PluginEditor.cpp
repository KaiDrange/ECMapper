#include "PluginProcessor.h"
#include "PluginEditor.h"

EigenharpMapperAudioProcessorEditor::EigenharpMapperAudioProcessorEditor (EigenharpMapperAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p) {
    setSize (800, 600);
    addAndMakeVisible(mainComponent);
    mainComponent.setBounds(getLocalBounds());
}

EigenharpMapperAudioProcessorEditor::~EigenharpMapperAudioProcessorEditor() {
}

void EigenharpMapperAudioProcessorEditor::resized() {
    mainComponent.setBounds(getLocalBounds());
}

Layout* EigenharpMapperAudioProcessorEditor::getLayout(DeviceType deviceType) {
    return mainComponent.getLayout(deviceType);
}
