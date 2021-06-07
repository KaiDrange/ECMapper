#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MainComponent.h"
#include "Models/Enums.h"

class ECMapperAudioProcessorEditor  : public juce::AudioProcessorEditor {
public:
    ECMapperAudioProcessorEditor(ECMapperAudioProcessor&);
    ~ECMapperAudioProcessorEditor() override;

    void resized() override;
//    Layout* getLayout(DeviceType deviceType);
    void recreateMainComponent();
    MainComponent *mainComponent;

private:
    ECMapperAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ECMapperAudioProcessorEditor)
};
