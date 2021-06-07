#include "PluginProcessor.h"
#include "PluginEditor.h"

ECMapperAudioProcessorEditor::ECMapperAudioProcessorEditor (ECMapperAudioProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p) {
    mainComponent = new MainComponent();
    setSize (800, 600);
    addAndMakeVisible(mainComponent);
    mainComponent->setBounds(getLocalBounds());
}

ECMapperAudioProcessorEditor::~ECMapperAudioProcessorEditor() {
    delete mainComponent;
}

void ECMapperAudioProcessorEditor::resized() {
    mainComponent->setBounds(getLocalBounds());
}

//Layout* ECMapperAudioProcessorEditor::getLayout(DeviceType deviceType) {
//    return mainComponent->getLayout(deviceType);
//}

void ECMapperAudioProcessorEditor::recreateMainComponent() {
    delete mainComponent;
    mainComponent = new MainComponent();
    addAndMakeVisible(mainComponent);
    mainComponent->setBounds(getLocalBounds());
//    mainComponent->addListener(&audioProcessor.getLayoutChangeHandler());
}
