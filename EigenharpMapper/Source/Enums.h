#pragma once

enum InstrumentType {
    Alpha = 1,
    Tau = 2,
    Pico = 3
};

enum KeyMappingType {
    None = 0,
    Note = 10,
    MidiCtrl = 20,
    Internal = 30
};

enum EigenharpKeyType {
    Normal = 1,
    Perc,
    Button
};

enum KeyColour {
    Off = 0,
    Green,
    Yellow,
    Red
};

enum Zone {
    NoZone = 0,
    Zone1,
    Zone2,
    Zone3,
    AllZones
};
