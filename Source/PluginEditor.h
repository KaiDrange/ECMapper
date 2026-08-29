#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

class ECMapperAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    ECMapperAudioProcessorEditor (ECMapperAudioProcessor&);
    ~ECMapperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ECMapperAudioProcessor& audioProcessor;
    std::unique_ptr<ecm::MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessorEditor)
};
