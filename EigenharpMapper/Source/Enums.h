#pragma once

enum InstrumentType {
    Alpha = 1,
    Tau = 2,
    Pico = 3
};

enum KeyMappingType {
    None = 0,
    Note = 10,
    MidiMsg = 20,
    Internal = 30
};

enum EigenharpKeyType {
    Normal = 1,
    Perc,
    Button
};

enum KeyColour {
    Off = 0x00000000,
    Green = 0xFF00FF00,
    Yellow = 0xFFFFFF00,
    Red = 0xFFFF0000
};

enum Zone {
    NoZone = 0,
    Zone1 = 1,
    Zone2 = 2,
    Zone3 = 3,
    AllZones = 4
};
