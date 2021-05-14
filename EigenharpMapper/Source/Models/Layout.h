#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "KeyConfig.h"


#define MAX_ROWS 5

class Layout {
public:
    Layout(InstrumentType instrumentType);
    ~Layout();

    void addKeyConfig(EigenharpKeyType keyType, int course, int key);
    juce::ValueTree getValueTree() const;
    void setValueTree(juce::ValueTree valueTree);
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
    KeyConfig* getKeyConfigs();
    
    void addListener(juce::ValueTree::Listener *listener);
    
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
    std::vector<KeyConfig> keyConfigs;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Layout)
};
