#pragma once
#include <JuceHeader.h>
#include "Enums.h"
#include "KeyConfig.h"


#define MAX_ROWS 5

class Layout {
public:
    Layout(DeviceType instrumentType, juce::ValueTree parentTree);
    ~Layout();

    void addKeyConfig(EigenharpKeyType keyType, int course, int key);
    juce::ValueTree getValueTree() const;
    int getNormalkeyCount() const;
    int getPercKeyCount() const;
    int getButtonCount() const;
    int getStripCount() const;
    int getKeyRowCount() const;
    const int* getKeyRowLengths() const;
    int getTotalKeyCount() const;
    int getPercKeyStartIndex() const;
    int getButtonStartIndex() const;
    DeviceType getDeviceType() const;
    
    juce::Identifier id_keyMapping = "keyMapping";
    juce::Identifier id_instrumentType = "instrumentType";
    juce::Identifier id_layout = "layout";
    juce::ValueTree valueTree;
    void addListener(juce::ValueTree::Listener *listener);

private:

    int normalKeyCount;
    int percKeyCount;
    int buttonCount;
    int stripCount;
    int keyRowCount;
    int keyRowLengths[MAX_ROWS];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Layout)
};
