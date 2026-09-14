if(NOT DEFINED copy_config)
    message(FATAL_ERROR "copy_config is required")
endif()

if(NOT copy_config STREQUAL "Release")
    message(STATUS "Skipping release artifact copy for ${copy_config} build")
    return()
endif()

if(NOT DEFINED copy_source OR NOT DEFINED copy_destination)
    message(FATAL_ERROR "copy_source and copy_destination are required")
endif()

set(copy_effective_codesign_identity "${copy_codesign_identity}")
if(NOT copy_effective_codesign_identity AND DEFINED ENV{ECMAPPER_MACOS_CODESIGN_IDENTITY})
    set(copy_effective_codesign_identity "$ENV{ECMAPPER_MACOS_CODESIGN_IDENTITY}")
endif()
if(NOT copy_effective_codesign_identity AND DEFINED ENV{OCTA_MACOS_CODESIGN_IDENTITY})
    set(copy_effective_codesign_identity "$ENV{OCTA_MACOS_CODESIGN_IDENTITY}")
endif()

function(ecmapper_is_mach_o_file path result_var)
    execute_process(
            COMMAND /usr/bin/file -b "${path}"
            OUTPUT_VARIABLE file_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE file_result
            ERROR_QUIET
    )

    if(file_result EQUAL 0 AND file_output MATCHES "Mach-O")
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(ecmapper_codesign_item path identity)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Cannot codesign missing path: ${path}")
    endif()

    if(IS_DIRECTORY "${path}")
        file(GLOB entries LIST_DIRECTORIES true "${path}/*")
        foreach(entry IN LISTS entries)
            if(IS_DIRECTORY "${entry}")
                ecmapper_codesign_item("${entry}" "${identity}")
            else()
                ecmapper_is_mach_o_file("${entry}" is_macho)
                if(is_macho)
                    execute_process(
                            COMMAND /usr/bin/codesign --force --timestamp --options runtime --sign "${identity}" "${entry}"
                            RESULT_VARIABLE sign_result
                            OUTPUT_VARIABLE sign_output
                            ERROR_VARIABLE sign_error
                    )
                    if(sign_result)
                        message(FATAL_ERROR "Failed to codesign ${entry}\n${sign_output}${sign_error}")
                    endif()
                endif()
            endif()
        endforeach()

        get_filename_component(directory_name "${path}" NAME)
        if(directory_name MATCHES "\\.(app|bundle|framework|xpc|appex|plugin|vst3)$")
            execute_process(
                    COMMAND /usr/bin/codesign --force --timestamp --options runtime --sign "${identity}" "${path}"
                    RESULT_VARIABLE sign_result
                    OUTPUT_VARIABLE sign_output
                    ERROR_VARIABLE sign_error
            )
            if(sign_result)
                message(FATAL_ERROR "Failed to codesign bundle ${path}\n${sign_output}${sign_error}")
            endif()
        endif()
    else()
        ecmapper_is_mach_o_file("${path}" is_macho)
        if(is_macho)
            execute_process(
                    COMMAND /usr/bin/codesign --force --timestamp --options runtime --sign "${identity}" "${path}"
                    RESULT_VARIABLE sign_result
                    OUTPUT_VARIABLE sign_output
                    ERROR_VARIABLE sign_error
            )
            if(sign_result)
                message(FATAL_ERROR "Failed to codesign ${path}\n${sign_output}${sign_error}")
            endif()
        endif()
    endif()
endfunction()

if(NOT EXISTS "${copy_source}")
    message(FATAL_ERROR "Release artifact not found: ${copy_source}")
endif()

file(MAKE_DIRECTORY "${copy_destination}")

get_filename_component(copy_name "${copy_source}" NAME)
set(copy_target "${copy_destination}/${copy_name}")

file(REMOVE_RECURSE "${copy_target}")

if(IS_DIRECTORY "${copy_source}")
    execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${copy_source}" "${copy_target}"
            RESULT_VARIABLE copy_result
            ERROR_VARIABLE copy_error
    )
else()
    execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${copy_source}" "${copy_target}"
            RESULT_VARIABLE copy_result
            ERROR_VARIABLE copy_error
    )
endif()

if(copy_result)
    message(FATAL_ERROR "Failed to copy release artifact to ${copy_target}\n${copy_error}")
endif()

if(APPLE AND IS_DIRECTORY "${copy_source}" AND copy_name MATCHES "\\.app$")
    set(info_plist "${copy_target}/Contents/Info.plist")
    if(EXISTS "${info_plist}")
        execute_process(
                COMMAND /usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "${info_plist}"
                OUTPUT_VARIABLE bundle_executable
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE plist_result
                ERROR_QUIET
        )

        if(plist_result EQUAL 0 AND bundle_executable STREQUAL "")
            message(FATAL_ERROR "CFBundleExecutable is empty in ${info_plist}")
        endif()

        if(plist_result EQUAL 0)
            file(GLOB macos_binaries "${copy_target}/Contents/MacOS/*")
            list(LENGTH macos_binaries macos_binary_count)

            if(macos_binary_count EQUAL 1)
                list(GET macos_binaries 0 macos_binary)
                get_filename_component(current_executable "${macos_binary}" NAME)

                if(NOT current_executable STREQUAL bundle_executable)
                    set(expected_executable "${copy_target}/Contents/MacOS/${bundle_executable}")
                    if(EXISTS "${expected_executable}")
                        file(REMOVE "${expected_executable}")
                    endif()

                    file(RENAME "${macos_binary}" "${expected_executable}")
                endif()
            endif()
        endif()
    endif()
endif()

if(APPLE AND IS_DIRECTORY "${copy_source}" AND copy_name MATCHES "\\.(app|bundle|framework|xpc|appex|plugin|vst3)$")
    if(NOT copy_effective_codesign_identity)
        message(FATAL_ERROR
                "A macOS codesign identity is required. Set ECMAPPER_MACOS_CODESIGN_IDENTITY or OCTA_MACOS_CODESIGN_IDENTITY before building release signing targets.")
    endif()

    message(STATUS "Codesigning ${copy_target} with Developer ID identity")
    ecmapper_codesign_item("${copy_target}" "${copy_effective_codesign_identity}")
endif()

message(STATUS "Copied release artifact to ${copy_target}")