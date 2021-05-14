#include "LayoutProcessorLookup.h"

void LayoutProcessorLookup::valueTreePropertyChanged(juce::ValueTree &vTree, const juce::Identifier &property) {
    std::cout << vTree.getType().toString() << std::endl;
}

void LayoutProcessorLookup::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childTree) {}
void LayoutProcessorLookup::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childTree, int removedAtIndex) {}
void LayoutProcessorLookup::valueTreeChildOrderChanged(juce::ValueTree &parentTree, juce::ValueTree &childTree, int oldIndex, int newIndex) {}
void LayoutProcessorLookup::valueTreeParentChanged(juce::ValueTree &vTree) {}
void LayoutProcessorLookup::valueTreeRedirected(juce::ValueTree &vTree) {}
