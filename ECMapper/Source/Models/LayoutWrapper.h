#pragma once

#include <JuceHeader.h>
#include "Enums.h"

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

    static LayoutKey getLayoutKey(KeyId keyId, juce::ValueTree &rootState);
    static void setLayoutKey(LayoutKey &key, juce::ValueTree &rootState);
    static void setKeyColour(KeyId &keyId, KeyColour keyColour, juce::ValueTree &rootState);
    static void setKeyType(KeyId &keyId, EigenharpKeyType keyType, juce::ValueTree &rootState);
    static void setKeyZone(KeyId &keyId, Zone zone, juce::ValueTree &rootState);
    static void setKeyMappingType(KeyId &keyId, KeyMappingType keyMappingType, juce::ValueTree &rootState);
    static void setKeyMappingValue(KeyId &keyId, juce::String keyMappingValue, juce::ValueTree &rootState);
    
    static juce::ValueTree getLayoutTree(DeviceType deviceType, juce::ValueTree &rootState);
    static LayoutKey getLayoutKeyFromKeyTree(juce::ValueTree keyTree);
    static DeviceType getDeviceTypeFromKeyTree(juce::ValueTree keyTree);
    static DeviceType getDeviceTypeFromLayoutTree(juce::ValueTree layoutTree);

    static void addListener(DeviceType deviceType, juce::ValueTree::Listener *listener, juce::ValueTree &rootState);
private:
    static juce::ValueTree getKeyTree(KeyId keyId, juce::ValueTree &rootState);
    static EigenharpKeyType getCorrectDefaultKeyType(DeviceType deviceType, int course, int keyNo);
    static KeyMappingType getDefaultMappingTypeFromKeyType(EigenharpKeyType keyType);

    static inline const LayoutKey default_key = {
        LayoutKey {
            .keyId = { .deviceType = DeviceType::None, .course = 0, .keyNo = 0 },
            .keyColour = KeyColour::Off,
            .keyType = EigenharpKeyType::Normal,
            .zone = Zone::Zone1,
            .keyMappingType = KeyMappingType::Note,
            .mappingValue = "0"
        }
    };

};
