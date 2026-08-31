cmake_minimum_required(VERSION 3.22)

project(ECMapper VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    enable_language(OBJC OBJCXX)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES x86_64 arm64)
    endif()
endif()

# ------------------------------------------------------------------------------
# Options

option(ECMAPPER_BUILD_APP "Build the standalone application" ON)
option(ECMAPPER_BUILD_PLUGIN "Build the VST3 plugin" ON)
if(WIN32)
    set(ECMAPPER_ENABLE_HARDWARE_DEFAULT OFF)
else()
    set(ECMAPPER_ENABLE_HARDWARE_DEFAULT ON)
endif()
option(ECMAPPER_ENABLE_HARDWARE "Enable direct hardware communication via EigenLite" ${ECMAPPER_ENABLE_HARDWARE_DEFAULT})

# ------------------------------------------------------------------------------
# Dependencies

add_subdirectory(JUCE)
if(ECMAPPER_ENABLE_HARDWARE)
    add_subdirectory(EigenLite/eigenapi)
else()
    add_library(eigenapi INTERFACE)
endif()

# Handle Universal Static Library for Pico Decoder on macOS
if(APPLE)
    set(PICO_ARM_LIB "${CMAKE_CURRENT_SOURCE_DIR}/EigenLite/resources/picodecoder/macOs/arm64/libpicodecoder_static.a")
    set(PICO_X64_LIB "${CMAKE_CURRENT_SOURCE_DIR}/EigenLite/resources/picodecoder/macOs/x86_64/libpicodecoder_static.a")
    set(PICO_UNIVERSAL_LIB "${CMAKE_CURRENT_BINARY_DIR}/libpicodecoder_static_universal.a")

    if(EXISTS "${PICO_ARM_LIB}" AND EXISTS "${PICO_X64_LIB}")
        add_custom_command(OUTPUT "${PICO_UNIVERSAL_LIB}"
            COMMAND lipo -create "${PICO_ARM_LIB}" "${PICO_X64_LIB}" -output "${PICO_UNIVERSAL_LIB}"
            DEPENDS "${PICO_ARM_LIB}" "${PICO_X64_LIB}"
            COMMENT "Creating universal static libpicodecoder"
        )
        add_custom_target(PicodecoderStaticUniversal ALL DEPENDS "${PICO_UNIVERSAL_LIB}")
        
        # Override the imported location for the target defined in EigenLite
        if(TARGET picodecoder_static)
            set_target_properties(picodecoder_static PROPERTIES IMPORTED_LOCATION "${PICO_UNIVERSAL_LIB}")
            add_dependencies(eigenapi PicodecoderStaticUniversal)
        endif()
    endif()
endif()

juce_add_binary_data(ECMapper_Data SOURCES
    Resources/GreenLight.png
    Resources/DarkLight.png
)

# ------------------------------------------------------------------------------
# Shared Configuration

file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS 
    Source/UI/*.cpp Source/UI/*.h 
    Source/Core/*.cpp Source/Core/*.h
)

set(COMMON_LIBS
    eigenapi
    ECMapper_Data
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_audio_basics # MIDI 2.0 / UMP support
    juce::juce_osc
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ------------------------------------------------------------------------------
# Standalone Application

if(ECMAPPER_BUILD_APP)
    juce_add_gui_app(ECMapper_App
        PRODUCT_NAME "ECMapper"
        VERSION "2.0.0"
        COMPANY_NAME "EigenLabs Alternative"
        BUNDLE_ID "com.yourcompany.ecmapper.app"
    )

    target_sources(ECMapper_App PRIVATE
        Source/StandaloneApp.cpp
        Source/StandaloneApp.h
        Source/StandaloneAppMainWindow.cpp
        Source/StandaloneAppMainWindow.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${SHARED_SOURCES}
    )

    target_link_libraries(ECMapper_App PRIVATE ${COMMON_LIBS} juce::juce_gui_extra)
    target_include_directories(ECMapper_App PRIVATE Source)
    target_compile_definitions(ECMapper_App PRIVATE ECMAPPER_ENABLE_HARDWARE=$<BOOL:${ECMAPPER_ENABLE_HARDWARE}>)
    juce_generate_juce_header(ECMapper_App)

    set_target_properties(ECMapper_App PROPERTIES OUTPUT_NAME "ECMapper")

    if(APPLE)
        set_target_properties(ECMapper_App PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourcompany.ecmapper.app"
            MACOSX_BUNDLE_BUNDLE_NAME "ECMapper"
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# Audio Plugin (VST3)

if(ECMAPPER_BUILD_PLUGIN)
    juce_add_plugin(ECMapper_Plugin
        PRODUCT_NAME "ECMapper"
        VERSION "2.0.0"
        COMPANY_NAME "EigenLabs Alternative"
        BUNDLE_ID "com.yourcompany.ecmapper.plugin"
        FORMATS VST3
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    target_sources(ECMapper_Plugin PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        ${SHARED_SOURCES}
    )

    target_link_libraries(ECMapper_Plugin PRIVATE ${COMMON_LIBS})
    target_compile_definitions(ECMapper_Plugin PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
    target_compile_definitions(ECMapper_Plugin PRIVATE ECMAPPER_ENABLE_HARDWARE=$<BOOL:${ECMAPPER_ENABLE_HARDWARE}>)
    target_include_directories(ECMapper_Plugin PRIVATE Source)
    juce_generate_juce_header(ECMapper_Plugin)

    set_target_properties(ECMapper_Plugin PROPERTIES OUTPUT_NAME "ECMapper")

    if(APPLE)
        set_target_properties(ECMapper_Plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourcompany.ecmapper.plugin"
            MACOSX_BUNDLE_BUNDLE_NAME "ECMapper"
        )
    endif()
endif()
