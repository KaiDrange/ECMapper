#pragma once

#include <JuceHeader.h>
#include "Enums.h"

namespace ecm {

class LayoutWrapper {
public:
    struct KeyId {
        int course = 0;
        int keyNo = 0;
        InstrumentType deviceType = InstrumentType::None;
        bool operator==(const KeyId& other) const noexcept { return course == other.course && keyNo == other.keyNo && deviceType == other.deviceType; }
        bool operator!=(const KeyId& other) const noexcept { return !(*this == other); }
        bool equals(const KeyId& id) const noexcept { return *this == id; }
    };
    
    struct LayoutKey {
        KeyId keyId;
        EigenharpKeyType keyType;
        KeyColour keyColour;
        Zone zone;
        KeyMappingType keyMappingType;
        juce::String mappingValue;
    };
    
    static inline const juce::Identifier id_layout { "layout" };
    static inline const juce::Identifier id_device { "device" };
    static inline const juce::Identifier id_key { "key" };
        
    static inline const juce::Identifier id_keyNo { "keyNo" };
    static inline const juce::Identifier id_course { "course" };
    static inline const juce::Identifier id_keyType { "keyType" };
    static inline const juce::Identifier id_keyColour { "keyColour" };
    static inline const juce::Identifier id_keyMappingType { "keyMappingType" };
    static inline const juce::Identifier id_mappingValue { "mappingValue" };
    static inline const juce::Identifier id_zone { "zone" };

    static LayoutKey getLayoutKey(KeyId keyId, juce::ValueTree& rootState);
    static void setLayoutKey(LayoutKey& key, juce::ValueTree& rootState);
    static void setKeyColour(KeyId keyId, KeyColour keyColour, juce::ValueTree& rootState);
    static void setKeyType(KeyId keyId, EigenharpKeyType keyType, juce::ValueTree& rootState);
    static void setKeyZone(KeyId keyId, Zone zone, juce::ValueTree& rootState);
    static void setKeyMappingType(KeyId keyId, KeyMappingType keyMappingType, juce::ValueTree& rootState);
    static void setKeyMappingValue(KeyId keyId, juce::String keyMappingValue, juce::ValueTree& rootState);
    
    static juce::ValueTree getLayoutTree(InstrumentType deviceType, juce::ValueTree& rootState);
    static LayoutKey getLayoutKeyFromKeyTree(juce::ValueTree keyTree);
    static InstrumentType getInstrumentTypeFromKeyTree(juce::ValueTree keyTree);
    static InstrumentType getInstrumentTypeFromLayoutTree(juce::ValueTree layoutTree);

    static void addListener(InstrumentType deviceType, juce::ValueTree::Listener* listener, juce::ValueTree& rootState);
private:
    static juce::ValueTree getKeyTree(KeyId keyId, juce::ValueTree& rootState);
    static EigenharpKeyType getCorrectDefaultKeyType(InstrumentType deviceType, int course, int keyNo);
    static KeyMappingType getDefaultMappingTypeFromKeyType(EigenharpKeyType keyType);
};

} // namespace ecm
