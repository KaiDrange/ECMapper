#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MainComponent.h"
#include "Models/Enums.h"

class EigenharpMapperAudioProcessorEditor  : public juce::AudioProcessorEditor {
public:
    EigenharpMapperAudioProcessorEditor(EigenharpMapperAudioProcessor&);
    ~EigenharpMapperAudioProcessorEditor() override;

    void resized() override;
    Layout* getLayout(DeviceType deviceType);
    void recreateMainComponent();
    MainComponent *mainComponent;

private:
    EigenharpMapperAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpMapperAudioProcessorEditor)
};
