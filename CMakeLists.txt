cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
        "Minimum macOS deployment version for ECMapper")

project(ECMapper_Dev VERSION 2.1.0 LANGUAGES C CXX)

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

if(APPLE)
    set(ECMAPPER_RELEASE_COPY_DIR_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/Release")
else()
    set(ECMAPPER_RELEASE_COPY_DIR_DEFAULT "")
endif()
set(ECMAPPER_RELEASE_COPY_DIR "${ECMAPPER_RELEASE_COPY_DIR_DEFAULT}" CACHE PATH
        "Directory that receives copied release build artifacts")
set(ECMAPPER_MACOS_CODESIGN_IDENTITY "" CACHE STRING
        "Developer ID Application identity used to sign copied macOS release bundles")
set(ECMAPPER_NOTARIZATION_KEYCHAIN_PROFILE "" CACHE STRING
        "Apple notarytool keychain profile used by the macOS notarization targets")

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

juce_add_binary_data(MetronomeSamples SOURCES assets/click1.wav assets/click2.wav)

file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS
        Source/UI/*.cpp Source/UI/*.h
        Source/Core/*.cpp Source/Core/*.h
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
        Source/Core/*.cpp Source/Core/*.h
)

set(COMMON_LIBS
        MetronomeSamples
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

function(ecmapper_add_release_copy target source destination)
    if(NOT ECMAPPER_RELEASE_COPY_DIR)
        return()
    endif()

    set(copy_script "${CMAKE_CURRENT_BINARY_DIR}/${target}_$<CONFIG>_release_copy.cmake")
    string(CONCAT copy_script_content
            "set(copy_config \"$<CONFIG>\")\n"
            "set(copy_source \"${source}\")\n"
            "set(copy_destination \"${destination}\")\n"
            "set(copy_codesign_identity \"${ECMAPPER_MACOS_CODESIGN_IDENTITY}\")\n"
            "include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_release_artifacts.cmake\")\n")

    file(GENERATE OUTPUT "${copy_script}" CONTENT "${copy_script_content}")

    add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -P "${copy_script}"
            VERBATIM)
endfunction()

function(ecmapper_add_release_notary_step target bundle action)
    if(NOT APPLE OR NOT ECMAPPER_RELEASE_COPY_DIR)
        return()
    endif()

    set(notarize_script "${CMAKE_CURRENT_BINARY_DIR}/${target}_$<CONFIG>_release_notary_step.cmake")
    string(CONCAT notarize_script_content
            "set(notarize_config \"$<CONFIG>\")\n"
            "set(notarize_bundle \"${bundle}\")\n"
            "set(notarize_action \"${action}\")\n"
            "set(notarize_keychain_profile \"${ECMAPPER_NOTARIZATION_KEYCHAIN_PROFILE}\")\n"
            "set(notarize_record_path \"${CMAKE_CURRENT_BINARY_DIR}/${target}_$<CONFIG>_notary.txt\")\n"
            "include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/notarize_release_artifacts.cmake\")\n")

    file(GENERATE OUTPUT "${notarize_script}" CONTENT "${notarize_script_content}")

    add_custom_target("${target}"
            COMMAND "${CMAKE_COMMAND}" -P "${notarize_script}"
            VERBATIM)
endfunction()

# ------------------------------------------------------------------------------
# Standalone Application

if(ECMAPPER_BUILD_APP)
    juce_add_gui_app(ECMapper_App
            PRODUCT_NAME "${PROJECT_NAME}"
            VERSION "${PROJECT_VERSION}"
            COMPANY_NAME "Tic Tech"
            BUNDLE_ID "com.tictech.ecmapper.dev.app"
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

    set_target_properties(ECMapper_App PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

    if(APPLE)
        set_target_properties(ECMapper_App PROPERTIES
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.tictech.ecmapper.dev.app"
                MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        )

        ecmapper_add_release_copy(
                ECMapper_App
                "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_App_artefacts/$<CONFIG>/${PROJECT_NAME}.app"
                "${ECMAPPER_RELEASE_COPY_DIR}"
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# Audio Plugin (VST3)

if(ECMAPPER_BUILD_PLUGIN)
    juce_add_plugin(ECMapper_Plugin
            PRODUCT_NAME "${PROJECT_NAME}"
            VERSION "${PROJECT_VERSION}"
            COMPANY_NAME "Tic Tech"
            BUNDLE_ID "com.tictech.ecmapper.dev.plugin"
            PLUGIN_CODE EcDv
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
            Source/UI/AppStyle.cpp
            Source/UI/AppStyle.h
            ${SHARED_SOURCES}
    )

    target_link_libraries(ECMapper_Plugin PRIVATE ${COMMON_LIBS})
    target_compile_definitions(ECMapper_Plugin PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
    target_compile_definitions(ECMapper_Plugin PRIVATE ECMAPPER_ENABLE_HARDWARE=$<BOOL:${ECMAPPER_ENABLE_HARDWARE}>)
    target_include_directories(ECMapper_Plugin PRIVATE Source)
    juce_generate_juce_header(ECMapper_Plugin)

    set_target_properties(ECMapper_Plugin PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

    if(APPLE)
        set_target_properties(ECMapper_Plugin PROPERTIES
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.tictech.ecmapper.dev.plugin"
                MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        )

        ecmapper_add_release_copy(
                ECMapper_Plugin_VST3
                "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_Plugin_artefacts/$<CONFIG>/VST3/${PROJECT_NAME}.vst3"
                "${ECMAPPER_RELEASE_COPY_DIR}"
        )
    endif()
endif()

if(APPLE AND ECMAPPER_RELEASE_COPY_DIR)
    if(ECMAPPER_BUILD_APP)
        ecmapper_add_release_notary_step(
                ECMapper_NotarizeApp
                "${ECMAPPER_RELEASE_COPY_DIR}/${PROJECT_NAME}.app"
                submit
        )
        add_dependencies(ECMapper_NotarizeApp ECMapper_App)

        ecmapper_add_release_notary_step(
                ECMapper_StapleApp
                "${ECMAPPER_RELEASE_COPY_DIR}/${PROJECT_NAME}.app"
                staple
        )
        add_dependencies(ECMapper_StapleApp ECMapper_App)
    endif()

    if(ECMAPPER_BUILD_PLUGIN)
        ecmapper_add_release_notary_step(
                ECMapper_NotarizePlugin
                "${ECMAPPER_RELEASE_COPY_DIR}/${PROJECT_NAME}.vst3"
                submit
        )
        add_dependencies(ECMapper_NotarizePlugin ECMapper_Plugin_VST3)

        ecmapper_add_release_notary_step(
                ECMapper_StaplePlugin
                "${ECMAPPER_RELEASE_COPY_DIR}/${PROJECT_NAME}.vst3"
                staple
        )
        add_dependencies(ECMapper_StaplePlugin ECMapper_Plugin_VST3)
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


juce_add_console_app(MPEZoneLayoutConfigurationTest
        PRODUCT_NAME "MPEZoneLayoutConfigurationTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.mpezonelayoutconfigurationtest"
)

target_sources(MPEZoneLayoutConfigurationTest PRIVATE
        Source/Tests/MPEZoneLayoutConfigurationTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(MPEZoneLayoutConfigurationTest PRIVATE ${COMMON_LIBS})
target_include_directories(MPEZoneLayoutConfigurationTest PRIVATE Source)
target_compile_definitions(MPEZoneLayoutConfigurationTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(MPEZoneLayoutConfigurationTest)


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


juce_add_console_app(Vst3OutputBusCountTest
        PRODUCT_NAME "Vst3OutputBusCountTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.vst3outputbuscounttest"
)

target_sources(Vst3OutputBusCountTest PRIVATE
        Source/Tests/Vst3OutputBusCountTest.cpp
)

target_link_libraries(Vst3OutputBusCountTest PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
target_include_directories(Vst3OutputBusCountTest PRIVATE Source)
target_compile_definitions(Vst3OutputBusCountTest PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        ECMAPPER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
juce_generate_juce_header(Vst3OutputBusCountTest)


juce_add_console_app(Vst3DirectEventRoutingTest
        PRODUCT_NAME "Vst3DirectEventRoutingTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.vst3directeventroutingtest"
)

target_sources(Vst3DirectEventRoutingTest PRIVATE
        Source/Tests/Vst3DirectEventRoutingTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(Vst3DirectEventRoutingTest PRIVATE ${COMMON_LIBS})
target_include_directories(Vst3DirectEventRoutingTest PRIVATE Source)
target_compile_definitions(Vst3DirectEventRoutingTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(Vst3DirectEventRoutingTest)


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


juce_add_console_app(CalibrationDialogThresholdRangeTest
        PRODUCT_NAME "CalibrationDialogThresholdRangeTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.calibrationdialogthresholdrangetest"
)

target_sources(CalibrationDialogThresholdRangeTest PRIVATE
        Source/Tests/CalibrationDialogThresholdRangeTest.cpp
        Source/UI/CalibrationDialogComponent.cpp
        Source/UI/AppStyle.cpp
        ${CORE_SOURCES}
)

target_link_libraries(CalibrationDialogThresholdRangeTest PRIVATE ${COMMON_LIBS})
target_include_directories(CalibrationDialogThresholdRangeTest PRIVATE Source)
target_compile_definitions(CalibrationDialogThresholdRangeTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(CalibrationDialogThresholdRangeTest)


juce_add_console_app(MainComponentMidi2ToggleTest
        PRODUCT_NAME "MainComponentMidi2ToggleTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.maincomponentmidi2toggletest"
)

target_sources(MainComponentMidi2ToggleTest PRIVATE
        Source/Tests/MainComponentMidi2ToggleTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${SHARED_SOURCES}
)

target_link_libraries(MainComponentMidi2ToggleTest PRIVATE ${COMMON_LIBS})
target_include_directories(MainComponentMidi2ToggleTest PRIVATE Source)
target_compile_definitions(MainComponentMidi2ToggleTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(MainComponentMidi2ToggleTest)

juce_add_console_app(HardwareServiceModeSanitizationTest
        PRODUCT_NAME "HardwareServiceModeSanitizationTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.hardwareservicemodesanitizationtest"
)

target_sources(HardwareServiceModeSanitizationTest PRIVATE
        Source/Tests/HardwareServiceModeSanitizationTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(HardwareServiceModeSanitizationTest PRIVATE ${COMMON_LIBS})
target_include_directories(HardwareServiceModeSanitizationTest PRIVATE Source)
target_compile_definitions(HardwareServiceModeSanitizationTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(HardwareServiceModeSanitizationTest)

juce_add_console_app(SettingsWrapperPresetListenerTest
        PRODUCT_NAME "SettingsWrapperPresetListenerTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.settingswrapperpresetlistenertest"
)

target_sources(SettingsWrapperPresetListenerTest PRIVATE
        Source/Tests/SettingsWrapperPresetListenerTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(SettingsWrapperPresetListenerTest PRIVATE ${COMMON_LIBS})
target_include_directories(SettingsWrapperPresetListenerTest PRIVATE Source)
target_compile_definitions(SettingsWrapperPresetListenerTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(SettingsWrapperPresetListenerTest)

juce_add_console_app(PluginStartupTransportDefaultsTest
        PRODUCT_NAME "PluginStartupTransportDefaultsTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.pluginstartuptransportdefaultstest"
)

target_sources(PluginStartupTransportDefaultsTest PRIVATE
        Source/Tests/PluginStartupTransportDefaultsTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${SHARED_SOURCES}
)

target_link_libraries(PluginStartupTransportDefaultsTest PRIVATE ${COMMON_LIBS})
target_include_directories(PluginStartupTransportDefaultsTest PRIVATE Source)
target_compile_definitions(PluginStartupTransportDefaultsTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(PluginStartupTransportDefaultsTest)

juce_add_console_app(MainMenuBarModelTest
        PRODUCT_NAME "MainMenuBarModelTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.mainmenubarmodeltest"
)

target_sources(MainMenuBarModelTest PRIVATE
        Source/Tests/MainMenuBarModelTest.cpp
        Source/UI/MainMenuBarModel.cpp
        Source/UI/MainMenuBarModel.h
        Source/UI/InputMidiReferenceText.cpp
        Source/UI/InputMidiReferenceText.h
)

target_link_libraries(MainMenuBarModelTest PRIVATE ${COMMON_LIBS})
target_include_directories(MainMenuBarModelTest PRIVATE Source)
target_compile_definitions(MainMenuBarModelTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(MainMenuBarModelTest)


juce_add_console_app(StandaloneInputMidiTransposeTest
        PRODUCT_NAME "StandaloneInputMidiTransposeTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.standaloneinputmiditransposetest"
)

target_sources(StandaloneInputMidiTransposeTest PRIVATE
        Source/Tests/StandaloneInputMidiTransposeTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${SHARED_SOURCES}
)

target_link_libraries(StandaloneInputMidiTransposeTest PRIVATE ${COMMON_LIBS})
target_include_directories(StandaloneInputMidiTransposeTest PRIVATE Source)
target_compile_definitions(StandaloneInputMidiTransposeTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(StandaloneInputMidiTransposeTest)

juce_add_console_app(LoggerMacroBehaviorTest
        PRODUCT_NAME "LoggerMacroBehaviorTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.loggermacrobehaviortest"
)

target_sources(LoggerMacroBehaviorTest PRIVATE
        Source/Tests/LoggerMacroBehaviorTest.cpp
        Source/Core/Logger.cpp
        Source/Core/Logger.h
)

target_link_libraries(LoggerMacroBehaviorTest PRIVATE ${COMMON_LIBS})
target_include_directories(LoggerMacroBehaviorTest PRIVATE Source)
target_compile_definitions(LoggerMacroBehaviorTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(LoggerMacroBehaviorTest)


juce_add_console_app(StripDirectionCalibrationTest
        PRODUCT_NAME "StripDirectionCalibrationTest"
        VERSION "1.0.0"
        COMPANY_NAME "Tic Tech"
        BUNDLE_ID "com.tictech.ecmapper.stripdirectioncalibrationtest"
)

target_sources(StripDirectionCalibrationTest PRIVATE
        Source/Tests/StripDirectionCalibrationTest.cpp
        ${CORE_SOURCES}
)

target_link_libraries(StripDirectionCalibrationTest PRIVATE ${COMMON_LIBS})
target_include_directories(StripDirectionCalibrationTest PRIVATE Source)
target_compile_definitions(StripDirectionCalibrationTest PRIVATE ECMAPPER_ENABLE_HARDWARE=0 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(StripDirectionCalibrationTest)


juce_add_console_app(EigenAudioBridgeTest PRODUCT_NAME "EigenAudioBridgeTest")
target_sources(EigenAudioBridgeTest PRIVATE
    Source/Tests/EigenAudioBridgeTest.cpp
    Source/Core/EigenAudioBridge.cpp
)
target_include_directories(EigenAudioBridgeTest PRIVATE Source)
target_link_libraries(EigenAudioBridgeTest PRIVATE
    MetronomeSamples
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_executable(USBTransferBuffersTest EigenLite/tests/USBTransferBuffersTest.cpp)
target_include_directories(USBTransferBuffersTest PRIVATE EigenLite/eigenapi)
