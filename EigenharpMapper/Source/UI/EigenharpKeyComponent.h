#pragma once

#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "Utility.h"
#include "../Models/KeyConfig.h"

class EigenharpKeyComponent  : public juce::DrawableButton {
public:
    EigenharpKeyComponent(const EigenharpKeyType keyType, const KeyConfig *keyConfig);
    ~EigenharpKeyComponent() override;

    void paint (juce::Graphics&) override;
    KeyConfig::KeyId getKeyId() const;

private:
    EigenharpKeyType keyType;
    const KeyConfig *keyConfig;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpKeyComponent)
};
