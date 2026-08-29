#include "PluginProcessor.h"
#include "PluginEditor.h"

ECMapperAudioProcessorEditor::ECMapperAudioProcessorEditor (ECMapperAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 300);
}

ECMapperAudioProcessorEditor::~ECMapperAudioProcessorEditor()
{
}

void ECMapperAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("ECMapper Combined - MIDI 2.0 Ready", getLocalBounds(), juce::Justification::centred, 1);
}

void ECMapperAudioProcessorEditor::resized()
{
}
