#pragma once

#include <JuceHeader.h>
#include "Enums.h"

extern juce::ValueTree *rootState;

class LayoutWrapper {
public:
    struct KeyId {
        int course = 0;
        int keyNo = 0;
        DeviceType deviceType = DeviceType::None;
        bool equals(KeyId &id) { return course == id.course && keyNo == id.keyNo && deviceType == id.deviceType; }
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

    static LayoutKey getLayoutKey(KeyId keyId);
    static void setLayoutKey(LayoutKey &key);
    static void setKeyColour(KeyId &keyId, KeyColour keyColour);
    static void setKeyType(KeyId &keyId, EigenharpKeyType keyType);
    static void setKeyZone(KeyId &keyId, Zone zone);
    static void setKeyMappingType(KeyId &keyId, KeyMappingType keyMappingType);
    static void setKeyMappingValue(KeyId &keyId, juce::String keyMappingValue);
    
    static juce::ValueTree getLayoutTree(DeviceType deviceType);
    static LayoutKey getLayoutKeyFromKeyTree(juce::ValueTree keyTree);
    static void addListener(DeviceType deviceType, juce::ValueTree::Listener *listener);
private:
    static juce::ValueTree getKeyTree(KeyId keyId);

    static inline const LayoutKey default_key = {
        LayoutKey {
            .keyId = { .deviceType = DeviceType::None, .course = 0, .keyNo = 0 },
            .keyColour = KeyColour::Off,
            .keyType = EigenharpKeyType::Normal,
            .zone = Zone::NoZone,
            .keyMappingType = KeyMappingType::None,
            .mappingValue = ""
        }
    };

};
