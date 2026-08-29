#pragma once

#include "PluginProcessor.h"

class ECMapperAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ECMapperAudioProcessorEditor (ECMapperAudioProcessor&);
    ~ECMapperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ECMapperAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessorEditor)
};
