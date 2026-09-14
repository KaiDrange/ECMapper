if(NOT DEFINED notarize_config)
    message(FATAL_ERROR "notarize_config is required")
endif()

if(NOT notarize_config STREQUAL "Release")
    return()
endif()

if(NOT DEFINED notarize_bundle OR notarize_bundle STREQUAL "")
    message(FATAL_ERROR "notarize_bundle is required")
endif()

if(NOT DEFINED notarize_action OR notarize_action STREQUAL "")
    message(FATAL_ERROR "notarize_action is required")
endif()

set(notarize_profile "${notarize_keychain_profile}")
if(NOT notarize_profile AND DEFINED ENV{ECMAPPER_NOTARIZATION_KEYCHAIN_PROFILE})
    set(notarize_profile "$ENV{ECMAPPER_NOTARIZATION_KEYCHAIN_PROFILE}")
endif()
if(NOT notarize_profile AND DEFINED ENV{OCTA_MACOS_NOTARY_PROFILE})
    set(notarize_profile "$ENV{OCTA_MACOS_NOTARY_PROFILE}")
endif()

if(NOT notarize_profile)
    message(FATAL_ERROR
            "A notary profile is required. Set ECMAPPER_NOTARIZATION_KEYCHAIN_PROFILE or OCTA_MACOS_NOTARY_PROFILE before running notarization targets.")
endif()

if(NOT EXISTS "${notarize_bundle}")
    message(FATAL_ERROR "Bundle does not exist: ${notarize_bundle}")
endif()

if(notarize_action STREQUAL "submit")
    if(NOT DEFINED ENV{TMPDIR} OR "$ENV{TMPDIR}" STREQUAL "")
        set(temp_root "/private/tmp")
    else()
        set(temp_root "$ENV{TMPDIR}")
    endif()

    get_filename_component(item_name "${notarize_bundle}" NAME)
    string(TIMESTAMP timestamp "%Y%m%d%H%M%S")
    set(zip_path "${temp_root}/${item_name}-${timestamp}.zip")

    file(REMOVE "${zip_path}")

    execute_process(
            COMMAND ditto -c -k --keepParent "${notarize_bundle}" "${zip_path}"
            RESULT_VARIABLE zip_result
            OUTPUT_VARIABLE zip_output
            ERROR_VARIABLE zip_error
    )
    if(NOT zip_result EQUAL 0)
        message(FATAL_ERROR "Failed to create notarization archive for ${notarize_bundle}\n${zip_output}${zip_error}")
    endif()

    message(STATUS "Notarization archive: ${zip_path}")

    execute_process(
            COMMAND xcrun notarytool submit "${zip_path}" --keychain-profile "${notarize_profile}" --output-format plist
            RESULT_VARIABLE notarize_result
            OUTPUT_FILE "${notarize_record_path}.plist"
            ERROR_VARIABLE notarize_error
    )
    file(REMOVE "${zip_path}")

    if(NOT notarize_result EQUAL 0)
        message(FATAL_ERROR "Notarization submission failed for ${notarize_bundle}\n${notarize_error}")
    endif()

    if(DEFINED notarize_record_path AND NOT notarize_record_path STREQUAL "")
        execute_process(
                COMMAND /usr/libexec/PlistBuddy -c "Print :id" "${notarize_record_path}.plist"
                RESULT_VARIABLE id_result
                OUTPUT_VARIABLE submission_id
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
        )
        if(NOT id_result EQUAL 0 OR submission_id STREQUAL "")
            set(submission_id "unknown")
        endif()

        file(WRITE "${notarize_record_path}"
                "bundle=${notarize_bundle}\nzip=${zip_path}\nsubmission_id=${submission_id}\nprofile=${notarize_profile}\n")
        message(STATUS "Notary record: ${notarize_record_path}")
        file(REMOVE "${notarize_record_path}.plist")
    endif()

    message(STATUS "Notarization submitted for ${notarize_bundle}")
    message(STATUS "Submission ID: ${submission_id}")
elseif(notarize_action STREQUAL "staple")
    execute_process(
            COMMAND xcrun stapler staple "${notarize_bundle}"
            RESULT_VARIABLE staple_result
            OUTPUT_VARIABLE staple_output
            ERROR_VARIABLE staple_error
    )
    if(NOT staple_result EQUAL 0)
        message(FATAL_ERROR "Failed to staple notarization ticket to ${notarize_bundle}\n${staple_output}${staple_error}")
    endif()
else()
    message(FATAL_ERROR "Unknown notarize_action: ${notarize_action}")
endif()