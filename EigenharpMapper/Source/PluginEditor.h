#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

//==============================================================================
/**
*/
class EigenharpMapperAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    EigenharpMapperAudioProcessorEditor (EigenharpMapperAudioProcessor&);
    ~EigenharpMapperAudioProcessorEditor() override;

    void resized() override;
    juce::ValueTree* getUISettings();

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    EigenharpMapperAudioProcessor& audioProcessor;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpMapperAudioProcessorEditor)
};
