#pragma once

#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "Utility.h"
#include "../Models/KeyConfig.h"

class KeyConfigComponent  : public juce::DrawableButton {
public:
    KeyConfigComponent(KeyConfig::KeyId id, EigenharpKeyType keyType, juce::ValueTree parentTree);
    ~KeyConfigComponent() override;

    void paint (juce::Graphics&) override;
    KeyConfig::KeyId getKeyId() const;
    KeyConfig keyConfig;

private:
    EigenharpKeyType keyType;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyConfigComponent)
};
