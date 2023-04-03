#pragma once

#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/LayoutWrapper.h"
#include "Utility.h"

class KeyConfigComponent : public juce::DrawableButton {
public:
    KeyConfigComponent(LayoutWrapper::KeyId id, EigenharpKeyType keyType, juce::AudioProcessorValueTreeState &pluginState);
    ~KeyConfigComponent() override;

    void paint (juce::Graphics&) override;
    LayoutWrapper::KeyId getKeyId() const;

private:
    EigenharpKeyType keyType;
    LayoutWrapper::KeyId keyId;
    juce::AudioProcessorValueTreeState &pluginState;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyConfigComponent)
};
