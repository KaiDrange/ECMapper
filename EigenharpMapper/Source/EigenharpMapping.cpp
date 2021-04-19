#include "EigenharpMapping.h"

EigenharpMapping::EigenharpMapping(InstrumentType instrumentType) {
    this->instrumentType = instrumentType;
    
    switch(instrumentType) {
        case InstrumentType::Alpha:
            keyCount = 120;
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
        case InstrumentType::Tau:
            keyCount = 72;
            percKeyCount = 12;
            keyRowCount = 4;
            keyRowLengths[0] = 16;
            keyRowLengths[1] = 16;
            keyRowLengths[2] = 20;
            keyRowLengths[3] = 20;
            buttonCount = 8;
            stripCount = 1;
            break;
        case InstrumentType::Pico:
            keyCount = 18;
            percKeyCount = 0;
            keyRowCount = 2;
            keyRowLengths[0] = 9;
            keyRowLengths[1] = 9;
            buttonCount = 4;
            stripCount = 1;
            break;
        default:
            break;
    }
    
    mappedKeys = new MappedKey[keyCount];
    mappedPercKeys = new MappedKey[percKeyCount];
    mappedButtons = new MappedKey[buttonCount];
}

EigenharpMapping::~EigenharpMapping() {
    delete[] mappedKeys;
    delete[] mappedPercKeys;
    delete[] mappedButtons;
}
