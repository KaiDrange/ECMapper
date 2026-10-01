# Can also be run independently with cmake -Drelease_bundle=/path/to/bundle -P this-file.
if(NOT DEFINED release_bundle OR NOT IS_DIRECTORY "${release_bundle}")
    message(FATAL_ERROR "release_bundle must name an existing macOS bundle")
endif()

function(ecmapper_release_check description)
    execute_process(COMMAND ${ARGN}
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "${description} failed for ${release_bundle}\n${output}${error}")
    endif()
    set(check_output "${output}${error}" PARENT_SCOPE)
endfunction()

ecmapper_release_check("Signature verification"
        /usr/bin/codesign --verify --deep --strict --verbose=2 "${release_bundle}")
ecmapper_release_check("Stapled notarization ticket verification"
        /usr/bin/xcrun stapler validate "${release_bundle}")
ecmapper_release_check("Reading bundle executable"
        /usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "${release_bundle}/Contents/Info.plist")
string(STRIP "${check_output}" executable_name)
set(main_executable "${release_bundle}/Contents/MacOS/${executable_name}")
if(executable_name STREQUAL "" OR NOT EXISTS "${main_executable}")
    message(FATAL_ERROR "Bundle executable is missing: ${main_executable}")
endif()

file(GLOB_RECURSE bundle_files LIST_DIRECTORIES false "${release_bundle}/Contents/*")
set(binary_count 0)
foreach(binary IN LISTS bundle_files)
    ecmapper_release_check("Inspecting bundle file" /usr/bin/file -b "${binary}")
    if(NOT check_output MATCHES "Mach-O")
        continue()
    endif()
    math(EXPR binary_count "${binary_count} + 1")
    foreach(arch IN ITEMS x86_64 arm64)
        ecmapper_release_check("${arch} architecture check for ${binary}"
                /usr/bin/lipo "${binary}" -verify_arch "${arch}")
        ecmapper_release_check("${arch} signing metadata check for ${binary}"
                /usr/bin/codesign -d --verbose=4 --arch "${arch}" "${binary}")
        if(NOT check_output MATCHES "Authority=Developer ID Application:"
                OR NOT check_output MATCHES "TeamIdentifier=[A-Z0-9]+"
                OR NOT check_output MATCHES "flags=[^\n]*runtime"
                OR NOT check_output MATCHES "Timestamp=")
            message(FATAL_ERROR
                    "${binary} (${arch}) requires a timestamped Developer ID Application signature with hardened runtime\n${check_output}")
        endif()

        ecmapper_release_check("${arch} deployment target check for ${binary}"
                /usr/bin/otool -arch "${arch}" -l "${binary}")
        if(check_output MATCHES "cmd LC_BUILD_VERSION[^\n]*\n[ \t]*cmdsize [0-9]+\n[ \t]*platform (macos|1)\n[ \t]*minos ([0-9.]+)")
            set(minimum_os "${CMAKE_MATCH_2}")
        elseif(check_output MATCHES "cmd LC_VERSION_MIN_MACOSX[^\n]*\n[ \t]*cmdsize [0-9]+\n[ \t]*version ([0-9.]+)")
            set(minimum_os "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR "Cannot read macOS minimum version for ${binary} (${arch})")
        endif()
        if(minimum_os VERSION_GREATER "13.0")
            message(FATAL_ERROR "${binary} (${arch}) requires macOS ${minimum_os}; release must support macOS 13.0")
        endif()
        if(binary STREQUAL main_executable AND NOT minimum_os VERSION_EQUAL "13.0")
            message(FATAL_ERROR "Main executable (${arch}) targets macOS ${minimum_os}; expected 13.0")
        endif()
        message(STATUS "Verified ${binary} (${arch}): minimum macOS ${minimum_os}")
    endforeach()
endforeach()
if(binary_count EQUAL 0)
    message(FATAL_ERROR "No Mach-O binaries found in ${release_bundle}")
endif()

message(STATUS "Release checks passed: ${release_bundle} — signed, notarization ticket validated, universal, macOS 13.0 compatible")
