# Example Audio Plugin CMakeLists.txt

# To get started on a new plugin, copy this entire folder (containing this file and C++ sources) to
# a convenient location, and then start making modifications.

# The first line of any CMake project should be a call to `cmake_minimum_required`, which checks
# that the installed CMake will be able to understand the following CMakeLists, and ensures that
# CMake's behaviour is compatible with the named version. This is a standard CMake command, so more
# information can be found in the CMake docs.

cmake_minimum_required(VERSION 3.15)

# The top-level CMakeLists.txt file for a project must contain a literal, direct call to the
# `project()` command. `project()` sets up some helpful variables that describe source/binary
# directories, and the current project version. This is a standard CMake command.

project(EigenCore VERSION 1.1.1)
set(BINARY_NAME "EigenCore v1.1.1b")

# If you've installed JUCE somehow (via a package manager, or directly using the CMake install
# target), you'll need to tell this project that it depends on the installed copy of JUCE. If you've
# included JUCE directly in your source tree (perhaps as a submodule), you'll need to tell CMake to
# include that subdirectory as part of the build.

# find_package(JUCE CONFIG REQUIRED)        # If you've installed JUCE to your system
# or
# add_subdirectory(JUCE)                    # If you've put JUCE in a subdirectory called JUCE

# If you are building a VST2 or AAX plugin, CMake needs to be told where to find these SDKs on your
# system. This setup should be done before calling `juce_add_plugin`.

# juce_set_aax_sdk_path(...)

# `juce_add_plugin` adds a static library target with the name passed as the first argument
# (AudioPluginExample here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.


if (NOT DISABLE_RPATH_OVERRIDE)
  ## setup rpath for linux and macos
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin") 
    message("adding Frameworks to rpath")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks;@loader_path/../Frameworks")
    set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)
  else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib")
  endif()
endif()



#include_directories("${PROJECT_SOURCE_DIR}/../common")

include_directories("${PROJECT_SOURCE_DIR}/EigenLite")

juce_add_plugin(EigenCore
        # VERSION ...                               # Set this if the plugin version is different to the project version
        ICON_BIG "../Gfx/EigenCore_Icon1024x1024.png" # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
        ICON_SMALL "../Gfx/EigenCore_Icon512x512.png"
        COMPANY_NAME "Tic Tech"                     # Specify the name of the plugin's author
        COMPANY_WEBSITE "https://ticticelectro.com/2023/07/17/ecmapper/"
        COMPANY_EMAIL "kaimdrange@hotmail.com"
        DESCRIPTION "EigenCore VST3 and Standalone"
        # IS_SYNTH TRUE/FALSE                       # Is this a synth or an effect?
        # NEEDS_MIDI_INPUT TRUE/FALSE               # Does the plugin need midi input?
        # NEEDS_MIDI_OUTPUT TRUE/FALSE              # Does the plugin need midi output?
        # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
        # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
        VST3_CATEGORIES "Tools"
        COPY_PLUGIN_AFTER_BUILD TRUE                # Should the plugin be installed to a default location after building?
        PLUGIN_MANUFACTURER_CODE TtEl                 # A four-character manufacturer id with at least one upper-case character
        PLUGIN_CODE Eyb4                            # A unique four-character plugin id with at least one upper-case character
        FORMATS  VST3  Standalone                   # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
        PRODUCT_NAME ${BINARY_NAME})                # The name of the final executable, which can differ from the target name

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

juce_generate_juce_header(EigenCore)

# `target_sources` adds source files to a target. We pass the target that needs the sources as the
# first argument, then a visibility parameter for the sources (PRIVATE is normally best practice,
# although it doesn't really affect executable targets). Finally, we supply a list of source files
# that will be built into the target. This is a standard CMake command.


# include(${CMAKE_CURRENT_SOURCE_DIR}/../common/CMakeLists.txt)

target_sources(EigenCore PRIVATE
        ./Source/Core/OSCCommunication.cpp
        ./Source/Core/EigenCore.cpp
        ./Source/Core/FirmwareReader.cpp
        ./Source/Core/OSCMessageQueue.cpp
        ./Source/Core/APICallback.cpp
        ./Source/PluginProcessor.cpp
        ./Source/PluginEditor.cpp
        )


# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(EigenCore
        PUBLIC
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0
        #JUCE_MODAL_LOOPS_PERMITTED=1
        )

# If your target needs extra binary assets, you can add them here. The first argument is the name of
# a new static library target that will include all the binary resources. There is an optional
# `NAMESPACE` argument that can specify the namespace of the generated binary data class. Finally,
# the SOURCES argument should be followed by a list of source files that should be built into the
# static library. These source files can be of any kind (wav data, images, fonts, icons etc.).
# Conversion to binary-data will happen when your target is built.

# juce_add_binary_data(AudioPluginData SOURCES ...)

juce_add_binary_data(EigenCore_DATA SOURCES
        "../EigenLite/resources/bs_mm_fw_0103.ihx"
        "../EigenLite/resources/psu_mm_fw_0102.ihx"
        "../EigenLite/resources/pico.ihx"
        "../GFX/GreenLight.png"
        "../GFX/DarkLight.png"
        )

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_audio_utils` module. Inter-module
# dependencies are resolved automatically, so `juce_core`, `juce_events` and so on will also be
# linked automatically. If we'd generated a binary data target above, we would need to link to it
# here too. This is a standard CMake command.

target_link_libraries(EigenCore PRIVATE
        EigenCore_DATA
        juce::juce_audio_utils
        juce::juce_osc
        eigenapi
        )



set_target_properties(${PROJECT_NAME}_Standalone PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME}_VST3 PROPERTIES PREFIX "")
add_custom_target(Picodecoder_Universal)
add_dependencies(EigenCore_Standalone Picodecoder_Universal)
add_dependencies(EigenCore_VST3 Picodecoder_Universal)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  message("Creating universal picodecoder and copying it to bundle")
  add_custom_command(TARGET Picodecoder_Universal
          PRE_BUILD COMMAND lipo -create
          "${CMAKE_CURRENT_BINARY_DIR}/../../EigenLite/resources/libpico_decoder_x86_64_1_0_0.dylib"
          "${CMAKE_CURRENT_BINARY_DIR}/../../EigenLite/resources/libpico_decoder_arm64_1_0_0.dylib"
          -output "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/libpicodecoder.dylib"
  )
  add_custom_command(TARGET EigenCore_Standalone
          PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/libpicodecoder.dylib"
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/Standalone/${BINARY_NAME}.app/Contents/Frameworks/libpicodecoder.dylib"
  )
  add_custom_command(TARGET EigenCore_VST3
          PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/libpicodecoder.dylib"
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/VST3/${BINARY_NAME}.vst3/Contents/Frameworks/libpicodecoder.dylib"
  )
endif()

# Code signing
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  message("Signing code as adhoc, i.e. '-'")
  add_custom_command(TARGET EigenCore_Standalone
          POST_BUILD COMMAND codesign -s "-"
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/Standalone/${BINARY_NAME}.app/Contents/Frameworks/libpicodecoder.dylib"
  )
  add_custom_command(TARGET EigenCore_Standalone
          POST_BUILD COMMAND codesign -s "-"
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/Standalone/${BINARY_NAME}.app"
  )
  add_custom_command(TARGET EigenCore_VST3
          POST_BUILD COMMAND codesign -s "-"
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/VST3/${BINARY_NAME}.vst3/Contents/Frameworks/libpicodecoder.dylib"
  )
  add_custom_command(TARGET EigenCore_VST3
          POST_BUILD COMMAND codesign -s "-"
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/VST3/${BINARY_NAME}.vst3"
  )
endif()

# Copy AFTER signing
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_custom_command(TARGET EigenCore_VST3
          POST_BUILD COMMAND cp -R
          "${CMAKE_CURRENT_BINARY_DIR}/EigenCore_artefacts/${CMAKE_BUILD_TYPE}/VST3/${BINARY_NAME}.vst3"
          "$ENV{HOME}/Library/Audio/Plug-ins/VST3/"
  )
endif()