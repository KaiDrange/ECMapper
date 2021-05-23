#include "Layout.h"

Layout::Layout(DeviceType instrumentType, juce::ValueTree parentTree) {
    valueTree = parentTree.getOrCreateChildWithName(id_layout + juce::String((int)instrumentType), nullptr);

    switch(instrumentType) {
        case DeviceType::Alpha:
            normalKeyCount = 120;
            percKeyCount = 12;
            keyRowCount = 5;
            keyRowLengths[0] = 24;
            keyRowLengths[1] = 24;
            keyRowLengths[2] = 24;
            keyRowLengths[3] = 24;
            keyRowLengths[4] = 24;
            buttonCount = 0;
            stripCount = 2;
            break;
        case DeviceType::Tau:
            normalKeyCount = 72;
            percKeyCount = 12;
            keyRowCount = 4;
            keyRowLengths[0] = 16;
            keyRowLengths[1] = 16;
            keyRowLengths[2] = 20;
            keyRowLengths[3] = 20;
            keyRowLengths[4] = 0;
            buttonCount = 8;
            stripCount = 1;
            break;
        case DeviceType::Pico:
            normalKeyCount = 18;
            percKeyCount = 0;
            keyRowCount = 2;
            keyRowLengths[0] = 9;
            keyRowLengths[1] = 9;
            keyRowLengths[2] = 0;
            keyRowLengths[3] = 0;
            keyRowLengths[4] = 0;
            buttonCount = 4;
            stripCount = 1;
            break;
        default:
            break;
    }
    

    valueTree.setProperty(id_instrumentType, (int)instrumentType, nullptr);
}

Layout::~Layout() {
}

juce::ValueTree Layout::getValueTree() const {
    return valueTree;
}

int Layout::getNormalkeyCount() const {
    return normalKeyCount;
}

int Layout::getPercKeyCount() const {
    return percKeyCount;
}

int Layout::getButtonCount() const {
    return buttonCount;
}

int Layout::getStripCount() const {
    return stripCount;
}

int Layout::getKeyRowCount() const {
    return keyRowCount;
}

const int* Layout::getKeyRowLengths() const {
    return keyRowLengths;
}

int Layout::getTotalKeyCount() const {
    return normalKeyCount + percKeyCount + buttonCount;
}

int Layout::getPercKeyStartIndex() const {
    return normalKeyCount;
}

int Layout::getButtonStartIndex() const {
    return normalKeyCount + percKeyCount;
}

DeviceType Layout::getDeviceType() const {
    return (DeviceType)valueTree[id_instrumentType].toString().getIntValue();
}

void Layout::addListener(juce::ValueTree::Listener *listener) {
    valueTree.addListener(listener);
}
