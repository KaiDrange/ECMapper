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
    Off = 0,
    Green = 1,
    Yellow = 2,
    Red = 3
};

enum Zone {
    NoZone = 0,
    Zone1 = 1,
    Zone2 = 2,
    Zone3 = 3,
    AllZones = 4
};