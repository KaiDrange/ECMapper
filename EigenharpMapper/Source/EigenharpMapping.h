#pragma once
#include <JuceHeader.h>
#include "Enums.h"

#define MAX_ROWS 5
#define MAX_KEYS 133

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

class EigenharpMapping {
public:
    EigenharpMapping(InstrumentType instrumentType);
    ~EigenharpMapping();

    void addMappedKey(EigenharpKeyType keyType);
    void logXML();
    int getNormalkeyCount() const;
    int getPercKeyCount() const;
    int getButtonCount() const;
    int getStripCount() const;
    int getKeyRowCount() const;
    const int* getKeyRowLengths() const;
    int getTotalKeyCount() const;
    int getPercKeyStartIndex() const;
    int getButtonStartIndex() const;
    InstrumentType getInstrumentType() const;
    MappedKey* getMappedKeys();
    
private:
    juce::ValueTree valueTree;
    juce::Identifier id_keyMapping = "keyMapping";
    juce::Identifier id_instrumentType = "instrumentType";

    int normalKeyCount;
    int percKeyCount;
    int buttonCount;
    int stripCount;
    int keyRowCount;
    int keyRowLengths[MAX_ROWS];
    //InstrumentType instrumentType;
    std::vector<MappedKey> mappedKeys;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EigenharpMapping)
};
