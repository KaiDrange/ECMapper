#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "MappedKey.h"


#define MAX_ROWS 5

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
