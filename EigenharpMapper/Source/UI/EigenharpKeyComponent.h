#pragma once

#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "Utility.h"
#include "../Models/KeyConfig.h"

class EigenharpKeyComponent  : public juce::DrawableButton {
public:
    EigenharpKeyComponent(KeyConfig::KeyId id, EigenharpKeyType keyType, juce::ValueTree parentTree);
    ~EigenharpKeyComponent() override;

    void paint (juce::Graphics&) override;
    KeyConfig::KeyId getKeyId() const;
    KeyConfig keyConfig;

private:
    EigenharpKeyType keyType;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpKeyComponent)
};
