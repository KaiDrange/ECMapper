//#pragma once
//#include <JuceHeader.h>
//#include "Enums.h"
//
//class KeyConfig {
//public:
//    struct KeyId {
//        int course = 0;
//        int keyNo = 0;
//        bool equals(KeyId &id) { return course == id.course && keyNo == id.keyNo; }
//    };
//    
//    KeyConfig(KeyConfig::KeyId keyId, EigenharpKeyType keyType, juce::ValueTree parentTree);
//    KeyConfig(juce::ValueTree keyTree);
//        
//    KeyId getKeyId() const;
//    void setKeyId(KeyId keyId);
//    EigenharpKeyType getKeyType() const;
//    void setKeyType(EigenharpKeyType keyType);
//    KeyColour getKeyColour() const;
//    void setKeyColour(KeyColour colour);
//    Zone getZone() const;
//    void setZone(Zone zone);
//    KeyMappingType getMappingType() const;
//    void setMappingType(KeyMappingType mappingType);
//    juce::String getMappingValue() const;
//    void setMappingValue(juce::String mappingValue);
//    juce::ValueTree getValueTree() const;
//
//private:
//    juce::Identifier id_keyNo = "keyNo";
//    juce::Identifier id_course = "course";
//    juce::Identifier id_keyType = "keyType";
//    juce::Identifier id_keyColour = "keyColour";
//    juce::Identifier id_keyMappingType = "keyMappingType";
//    juce::Identifier id_mappingValue = "mappingValue";
//    juce::Identifier id_zone = "zone";
//    juce::Identifier id_key = "key";
//    juce::ValueTree valueTree;
//    
//};
