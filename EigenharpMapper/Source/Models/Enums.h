#pragma once

enum class DeviceType {
    None = 0,
    Alpha = 1,
    Tau = 2,
    Pico = 3
};

enum class KeyMappingType {
    None = 0,
    Note = 10,
    MidiMsg = 20,
    Internal = 30
};

enum class EigenharpKeyType {
    Normal = 1,
    Perc,
    Button
};

enum class KeyColour {
    Off = 0,
    Green = 1,
    Red = 2,
    Yellow = 3
};

enum class Zone {
    NoZone = 0,
    Zone1 = 1,
    Zone2 = 2,
    Zone3 = 3,
    AllZones = 4
};

enum class MidiChannelType {
    Chan1 = 1, Chan2, Chan3, Chan4, Chan5, Chan6, Chan7, Chan8, Chan9, Chan10, Chan11, Chan12, Chan13, Chan14, Chan15, Chan16,
    MPE1_16,
    MPE1_14,
    MPE1_12,
    MPE1_8,
    MPE9_16,
    MPE13_16
};

enum class MidiValueType {
    CC = 1,
    Pitchbend,
    ChannelAftertouch,
    PolyAftertouch,
    Off
};
