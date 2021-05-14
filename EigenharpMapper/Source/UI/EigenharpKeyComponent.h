#pragma once

#include <JuceHeader.h>
#include "../Models/Enums.h"
#include "../Models/Layout.h"
#include "Utility.h"
#include "../Models/MappedKey.h"

class EigenharpKeyComponent  : public juce::DrawableButton {
public:
    EigenharpKeyComponent(const EigenharpKeyType keyType, const MappedKey *mappedKey);
    ~EigenharpKeyComponent() override;

    void paint (juce::Graphics&) override;
    MappedKey::KeyId getKeyId() const;

private:
    EigenharpKeyType keyType;
    const MappedKey *mappedKey;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpKeyComponent)
};
