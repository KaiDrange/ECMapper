cmake_minimum_required(VERSION 3.22)

project(ECMapper VERSION 2.0.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES x86_64 arm64)
    endif()
endif()

# ------------------------------------------------------------------------------
# Options

option(ECMAPPER_BUILD_APP "Build the standalone application" ON)
option(ECMAPPER_BUILD_PLUGIN "Build the VST3 plugin" ON)

# ------------------------------------------------------------------------------
# Dependencies

add_subdirectory(JUCE)
add_subdirectory(EigenLite/eigenapi)

juce_add_binary_data(ECMapper_Data SOURCES
    Resources/GreenLight.png
    Resources/DarkLight.png
    EigenLite/resources/bs_mm_fw_0103.ihx
    EigenLite/resources/psu_mm_fw_0102.ihx
    EigenLite/resources/pico.ihx
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
    juce_generate_juce_header(ECMapper_App)

    set_target_properties(ECMapper_App PROPERTIES OUTPUT_NAME "ECMapper")

    if(APPLE)
        set_target_properties(ECMapper_App PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourcompany.ecmapper.app"
            MACOSX_BUNDLE_BUNDLE_NAME "ECMapper"
        )

        # Universal Picodecoder handling
        add_custom_target(Picodecoder_App_Universal)
        add_dependencies(ECMapper_App Picodecoder_App_Universal)
        
        add_custom_command(TARGET Picodecoder_App_Universal
            PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_App_artefacts"
            COMMAND lipo -create
            "${CMAKE_CURRENT_SOURCE_DIR}/EigenLite/resources/libpico_decoder_x86_64_1_0_0.dylib"
            "${CMAKE_CURRENT_SOURCE_DIR}/EigenLite/resources/libpico_decoder_arm64_1_0_0.dylib"
            -output "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_App_artefacts/libpicodecoder.dylib"
            COMMENT "Creating universal libpicodecoder.dylib"
        )

        add_custom_command(TARGET ECMapper_App
            PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_App_artefacts/libpicodecoder.dylib"
            "$<TARGET_FILE_DIR:ECMapper_App>/../Frameworks/libpicodecoder.dylib"
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
    target_include_directories(ECMapper_Plugin PRIVATE Source)
    juce_generate_juce_header(ECMapper_Plugin)

    set_target_properties(ECMapper_Plugin PROPERTIES OUTPUT_NAME "ECMapper")

    if(APPLE)
        set_target_properties(ECMapper_Plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourcompany.ecmapper.plugin"
            MACOSX_BUNDLE_BUNDLE_NAME "ECMapper"
        )

        # Universal Picodecoder handling for plugin
        add_custom_target(Picodecoder_Plugin_Universal)
        add_dependencies(ECMapper_Plugin Picodecoder_Plugin_Universal)
        
        add_custom_command(TARGET Picodecoder_Plugin_Universal
            PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_Plugin_artefacts"
            COMMAND lipo -create
            "${CMAKE_CURRENT_SOURCE_DIR}/EigenLite/resources/libpico_decoder_x86_64_1_0_0.dylib"
            "${CMAKE_CURRENT_SOURCE_DIR}/EigenLite/resources/libpico_decoder_arm64_1_0_0.dylib"
            -output "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_Plugin_artefacts/libpicodecoder.dylib"
            COMMENT "Creating universal libpicodecoder.dylib for plugin"
        )

        add_custom_command(TARGET ECMapper_Plugin
            PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/ECMapper_Plugin_artefacts/libpicodecoder.dylib"
            "$<TARGET_FILE_DIR:ECMapper_Plugin>/../Frameworks/libpicodecoder.dylib"
        )
    endif()
endif()
