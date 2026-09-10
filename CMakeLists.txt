cmake_minimum_required(VERSION 3.22)

project(ECMapper VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32 AND (MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    message(FATAL_ERROR
            "ECMapper cannot be configured with MinGW on Windows. "
            "JUCE does not support that toolchain here. "
            "Switch CLion to a Visual Studio or clang-cl toolchain, then reconfigure "
            "using the windows-debug or windows-release preset.")
endif()

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

# ------------------------------------------------------------------------------
# Shared Configuration

file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS
        Source/UI/*.cpp Source/UI/*.h
        Source/Core/*.cpp Source/Core/*.h
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        Source/Core/*.cpp Source/Core/*.h
)

set(COMMON_LIBS
        eigenapi
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        juce::juce_audio_basics # MIDI 2.0 / UMP support
        juce::juce_midi_ci
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
            COMPANY_NAME "Tic Tech"
            BUNDLE_ID "com.tictech.ecmapper.app"
            ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/app_icon.png"
    )

    target_sources(ECMapper_App PRIVATE
            Source/StandaloneApp.cpp
            Source/StandaloneApp.h
            Source/StandaloneAppMainWindow.cpp
            Source/StandaloneAppMainWindow.h
            Source/UI/AppStyle.cpp
            Source/UI/AppStyle.h
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
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.tictech.ecmapper.app"
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
            COMPANY_NAME "Tic Tech"
            BUNDLE_ID "com.tictech.ecmapper.plugin"
            FORMATS VST3
            IS_SYNTH TRUE
            NEEDS_MIDI_INPUT TRUE
            NEEDS_MIDI_OUTPUT TRUE
            COPY_PLUGIN_AFTER_BUILD TRUE
    )

    target_sources(ECMapper_Plugin PRIVATE
            Source/PluginProcessor.cpp
            Source/PluginProcessor.h
            Source/PluginEditor.cpp
            Source/PluginEditor.h
            Source/UI/AppStyle.cpp
            Source/UI/AppStyle.h
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
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.tictech.ecmapper.plugin"
                MACOSX_BUNDLE_BUNDLE_NAME "ECMapper"
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# MIDI Receiver Utility
# ------------------------------------------------------------------------------

juce_add_console_app(MidiReceiver
        PRODUCT_NAME "MidiReceiver"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.midireceiver.app"
)

target_sources(MidiReceiver PRIVATE Source/Utility/MidiReceiver/Main.cpp)
target_link_libraries(MidiReceiver PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_basics
        juce::juce_gui_basics
        juce::juce_events
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
target_compile_definitions(MidiReceiver PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(MidiReceiver)

juce_add_console_app(OSCBridgeLifecycleTest
        PRODUCT_NAME "OSCBridgeLifecycleTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.oscbridgelifecycletest"
)

target_sources(OSCBridgeLifecycleTest PRIVATE
        Source/Tests/OSCBridgeLifecycleTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(OSCBridgeLifecycleTest PRIVATE ${COMMON_LIBS})
target_include_directories(OSCBridgeLifecycleTest PRIVATE Source)
target_compile_definitions(OSCBridgeLifecycleTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(OSCBridgeLifecycleTest)

juce_add_console_app(RollPreCurveTest
        PRODUCT_NAME "RollPreCurveTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.rollprecurvetest"
)

target_sources(RollPreCurveTest PRIVATE
        Source/Tests/RollPreCurveTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(RollPreCurveTest PRIVATE ${COMMON_LIBS})
target_include_directories(RollPreCurveTest PRIVATE Source)
target_compile_definitions(RollPreCurveTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(RollPreCurveTest)


juce_add_console_app(NoteOnExpressionTransitionTest
        PRODUCT_NAME "NoteOnExpressionTransitionTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.noteonexpressiontransitiontest"
)

target_sources(NoteOnExpressionTransitionTest PRIVATE
        Source/Tests/NoteOnExpressionTransitionTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(NoteOnExpressionTransitionTest PRIVATE ${COMMON_LIBS})
target_include_directories(NoteOnExpressionTransitionTest PRIVATE Source)
target_compile_definitions(NoteOnExpressionTransitionTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(NoteOnExpressionTransitionTest)


juce_add_console_app(ReleaseVelocityConsistencyTest
        PRODUCT_NAME "ReleaseVelocityConsistencyTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.releasevelocityconsistencytest"
)

target_sources(ReleaseVelocityConsistencyTest PRIVATE
        Source/Tests/ReleaseVelocityConsistencyTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(ReleaseVelocityConsistencyTest PRIVATE ${COMMON_LIBS})
target_include_directories(ReleaseVelocityConsistencyTest PRIVATE Source)
target_compile_definitions(ReleaseVelocityConsistencyTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(ReleaseVelocityConsistencyTest)


juce_add_console_app(BreathStabilityTest
        PRODUCT_NAME "BreathStabilityTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.breathstabilitytest"
)

target_sources(BreathStabilityTest PRIVATE
        Source/Tests/BreathStabilityTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(BreathStabilityTest PRIVATE ${COMMON_LIBS})
target_include_directories(BreathStabilityTest PRIVATE Source)
target_compile_definitions(BreathStabilityTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(BreathStabilityTest)


juce_add_console_app(CalibrationListenerTest
        PRODUCT_NAME "CalibrationListenerTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.calibrationlistenertest"
)

target_sources(CalibrationListenerTest PRIVATE
        Source/Tests/CalibrationListenerTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(CalibrationListenerTest PRIVATE ${COMMON_LIBS})
target_include_directories(CalibrationListenerTest PRIVATE Source)
target_compile_definitions(CalibrationListenerTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(CalibrationListenerTest)


juce_add_console_app(LayoutLoadListenerTest
        PRODUCT_NAME "LayoutLoadListenerTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.layoutloadlistenertest"
)

target_sources(LayoutLoadListenerTest PRIVATE
        Source/Tests/LayoutLoadListenerTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(LayoutLoadListenerTest PRIVATE ${COMMON_LIBS})
target_include_directories(LayoutLoadListenerTest PRIVATE Source)
target_compile_definitions(LayoutLoadListenerTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(LayoutLoadListenerTest)

