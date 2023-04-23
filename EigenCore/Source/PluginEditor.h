#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Core/Enums.h"

class EigenCoreAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    EigenCoreAudioProcessorEditor (EigenCoreAudioProcessor&);
    ~EigenCoreAudioProcessorEditor() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    
private:
    juce::Image ledGreen;
    juce::Image ledOff;
    EigenCoreAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenCoreAudioProcessorEditor)
};
