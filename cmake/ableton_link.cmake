include(FetchContent)

# Link is header-only. Populate its SDK and Asio without building upstream's
# examples/tests or applying their project-wide compiler settings.
FetchContent_Declare(ableton_link
        GIT_REPOSITORY https://github.com/Ableton/link.git
        GIT_TAG 9c9091275e707ab09d09a5a608fcdb84bf0dec85 # Link-4.1
        GIT_SUBMODULES modules/asio-standalone
        GIT_SUBMODULES_RECURSE TRUE
        SOURCE_SUBDIR ecmapper-sdk-only
)
FetchContent_MakeAvailable(ableton_link)

# Upstream's config sets CMAKE_CXX_STANDARD to 17; preserve ECMapper's standard.
set(ecmapper_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
include("${ableton_link_SOURCE_DIR}/AbletonLinkConfig.cmake")
set(CMAKE_CXX_STANDARD "${ecmapper_saved_cxx_standard}")
unset(ecmapper_saved_cxx_standard)

if(WIN32)
    # Match JUCE's Windows 10 baseline in translation units including Link alone.
    set_property(TARGET Ableton::Link APPEND PROPERTY
            INTERFACE_COMPILE_DEFINITIONS _WIN32_WINNT=0x0A00)
endif()
