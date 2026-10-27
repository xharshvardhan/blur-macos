# [Blur2PC][macOS] Stage a local app using CMake's maintained Mach-O dependency bundler.
# The destination must be new. Game data is deliberately outside the source app.
cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED BLUR_SOURCE_APP OR NOT EXISTS "${BLUR_SOURCE_APP}/Contents/MacOS/BlurNativeGame")
    message(FATAL_ERROR "Build BlurNativeGame before staging its app bundle")
endif()
if(NOT DEFINED BLUR_STAGE_DIR OR EXISTS "${BLUR_STAGE_DIR}")
    message(FATAL_ERROR "BLUR_STAGE_DIR must name a new destination directory")
endif()
file(MAKE_DIRECTORY "${BLUR_STAGE_DIR}")
file(COPY "${BLUR_SOURCE_APP}" DESTINATION "${BLUR_STAGE_DIR}")
get_filename_component(_app_name "${BLUR_SOURCE_APP}" NAME)
set(_app "${BLUR_STAGE_DIR}/${_app_name}")
include(BundleUtilities)
# BundleUtilities follows transitive LC_LOAD_DYLIB entries and preserves system
# libraries/frameworks. Do not hand-maintain FFmpeg's dependency closure.
fixup_bundle("${_app}" "" "/opt/homebrew/lib;/usr/local/lib")
file(GLOB_RECURSE _bundled_dylibs "${_app}/Contents/Frameworks/*.dylib")
# The host package manager may build libraries for a newer OS than the game's
# compile flag. Derive the launch requirement from the actual Mach-O inputs.
set(_minimum_os "0")
foreach(_mach_o IN ITEMS "${_app}/Contents/MacOS/BlurNativeGame" ${_bundled_dylibs})
    execute_process(COMMAND /usr/bin/otool -l "${_mach_o}" OUTPUT_VARIABLE _load_commands
        COMMAND_ERROR_IS_FATAL ANY)
    string(REGEX MATCHALL "minos [0-9]+([.][0-9]+)*" _versions "${_load_commands}")
    foreach(_version IN LISTS _versions)
        string(REPLACE "minos " "" _version "${_version}")
        if(_version VERSION_GREATER _minimum_os)
            set(_minimum_os "${_version}")
        endif()
    endforeach()
endforeach()
if(_minimum_os STREQUAL "0")
    message(FATAL_ERROR "Could not determine the bundled Mach-O deployment requirement")
endif()
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Delete :LSMinimumSystemVersion" "${_app}/Contents/Info.plist"
    OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND /usr/libexec/PlistBuddy -c "Add :LSMinimumSystemVersion string ${_minimum_os}" "${_app}/Contents/Info.plist"
    COMMAND_ERROR_IS_FATAL ANY)
foreach(_library IN LISTS _bundled_dylibs)
    if(NOT IS_SYMLINK "${_library}")
        execute_process(COMMAND /usr/bin/codesign --force --sign - "${_library}"
            COMMAND_ERROR_IS_FATAL ANY)
    endif()
endforeach()
execute_process(COMMAND /usr/bin/codesign --force --sign - "${_app}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND /usr/bin/codesign --verify --deep --strict "${_app}"
    COMMAND_ERROR_IS_FATAL ANY)
message(STATUS "Staged local development app: ${_app}")
message(STATUS "Minimum bundled runtime: macOS ${_minimum_os}. No game data, notarization or release is included.")
