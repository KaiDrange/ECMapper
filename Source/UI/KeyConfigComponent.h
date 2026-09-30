#pragma once
#include <JuceHeader.h>
#include "../Core/Enums.h"
#include "../Core/LayoutWrapper.h"

namespace ecm {

class KeyConfigComponent : public juce::DrawableButton {
public:
    KeyConfigComponent(LayoutWrapper::KeyId id, EigenharpKeyType keyType, juce::AudioProcessorValueTreeState& pluginState);
    ~KeyConfigComponent() override = default;

    LayoutWrapper::KeyId getKeyId() const { return keyId; }
    void refreshFromState();

protected:
    void paint (juce::Graphics& g) override;

private:
    EigenharpKeyType keyType;
    LayoutWrapper::KeyId keyId;
    juce::AudioProcessorValueTreeState& pluginState;
    LayoutWrapper::LayoutKey displayedKey {};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyConfigComponent)
};

} // namespace ecm
