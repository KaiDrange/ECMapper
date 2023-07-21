cmake_minimum_required(VERSION 3.15)
project(ECMAPPER)

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message("apple config")

    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES x86_64 arm64)
    endif()
    message(STATUS "CMAKE_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES}")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if (CMAKE_CROSSCOMPILING)
    message("cross compile config")
  else ()
   set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
   set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-a17 -mfloat-abi=hard -mfpu=neon-vfpv4")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-a17 -mfloat-abi=hard -mfpu=neon-vfpv4")
  endif()
endif()


add_subdirectory(JUCE)


add_subdirectory(EigenLite/eigenapi)
add_subdirectory(ECMapper)
add_subdirectory(EigenCore)

# set(CMAKE_FOLDER .)
function(_juce_add_pips)
    file(GLOB headers
        CONFIGURE_DEPENDS LIST_DIRECTORIES false
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    endif()

    foreach(header IN ITEMS ${headers})
        juce_add_pip(${header} added_target)
        target_link_libraries(${added_target} PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
    endforeach()
endfunction()

