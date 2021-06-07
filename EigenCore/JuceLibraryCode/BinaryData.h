/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   EigenCore_Icon512x512_png;
    const int            EigenCore_Icon512x512_pngSize = 63182;

    extern const char*   EigenCore_Icon1024x1024_png;
    const int            EigenCore_Icon1024x1024_pngSize = 63569;

    extern const char*   _69eigenharp_rules;
    const int            _69eigenharp_rulesSize = 767;

    extern const char*   bs_mm_fw_0103_ihx;
    const int            bs_mm_fw_0103_ihxSize = 20326;

    extern const char*   libeigenapi_dylib;
    const int            libeigenapi_dylibSize = 331344;

    extern const char*   libpicodecoder_dylib;
    const int            libpicodecoder_dylibSize = 60216;

    extern const char*   pico_ihx;
    const int            pico_ihxSize = 24114;

    extern const char*   psu_mm_fw_0102_ihx;
    const int            psu_mm_fw_0102_ihxSize = 17752;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 8;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
