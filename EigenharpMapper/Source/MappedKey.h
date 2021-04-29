#pragma once
#include <JuceHeader.h>
#include "Enums.h"

class MappedKey {
public:
    MappedKey(EigenharpKeyType keyType, juce::ValueTree &rootValueTree);
    
    juce::String getKeyId() const;
    void setKeyId(juce::String keyId);
    EigenharpKeyType getKeyType() const;
    void setKeyType(EigenharpKeyType keyType);
    KeyColour getKeyColour() const;
    void setKeyColour(KeyColour colour);
    Zone getZone() const;
    void setZone(Zone zone);
    KeyMappingType getMappingType() const;
    void setMappingType(KeyMappingType mappingType);
    juce::String getMappingValue() const;
    void setMappingValue(juce::String mappingValue);

private:
    juce::Identifier id_keyId = "keyId";
    juce::Identifier id_keyType = "keyType";
    juce::Identifier id_keyColour = "keyColour";
    juce::Identifier id_keyMappingType = "keyMappingType";
    juce::Identifier id_mappingValue = "mappingValue";
    juce::Identifier id_zone = "zone";
    
    juce::ValueTree valueTree;
};
