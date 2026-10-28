# [Blur2PC][macOS] Stage a development app with CMake's maintained Mach-O bundler.
# New destination only; source app/game data remain unchanged. No notarization/release claim.
cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED BLUR_SOURCE_APP OR NOT EXISTS "${BLUR_SOURCE_APP}/Contents/MacOS/BlurNativeGame")
    message(FATAL_ERROR "Build BlurNativeGame before staging its raw app bundle")
endif()
if(NOT DEFINED BLUR_STAGE_DIR OR EXISTS "${BLUR_STAGE_DIR}" OR IS_SYMLINK "${BLUR_STAGE_DIR}")
    message(FATAL_ERROR "BLUR_STAGE_DIR must name a new destination directory")
endif()
file(REAL_PATH "${BLUR_SOURCE_APP}" _source)
file(REAL_PATH "${BLUR_STAGE_DIR}" _stage)
cmake_path(IS_PREFIX _source "${_stage}" NORMALIZE _stage_inside_source)
if(_stage_inside_source)
    message(FATAL_ERROR "The stage destination must be outside the unchanged source app")
endif()
get_filename_component(_source_parent "${_source}" DIRECTORY)
get_filename_component(_app_name "${_source}" NAME)
if(NOT DEFINED BLUR_NATIVE_LICENSE_DIR)
    set(BLUR_NATIVE_LICENSE_DIR "${_source_parent}/licenses")
endif()
if(NOT DEFINED BLUR_BUNDLE_IDENTIFIER)
    set(BLUR_BUNDLE_IDENTIFIER "io.github.xharshvardhan.blur-macos.development")
endif()
if(NOT DEFINED BLUR_BUNDLE_NAME)
    set(BLUR_BUNDLE_NAME "Blur Native Development")
endif()
if(NOT DEFINED BLUR_BUNDLE_VERSION)
    set(BLUR_BUNDLE_VERSION "0.1.0")
endif()
if(NOT DEFINED BLUR_BUNDLE_BUILD_VERSION)
    set(BLUR_BUNDLE_BUILD_VERSION "1")
endif()
# Preserve the source app's Game Mode policy unless explicitly requested.
if(NOT DEFINED BLUR_BUNDLE_GAME_MODE)
    set(BLUR_BUNDLE_GAME_MODE OFF)
endif()
set(_game_mode_args)
if(BLUR_BUNDLE_GAME_MODE)
    list(APPEND _game_mode_args --game-mode)
endif()
find_program(BLUR_BUNDLE_PYTHON NAMES python3 REQUIRED)
set(_notice_helper "${CMAKE_CURRENT_LIST_DIR}/bundle_notices.py")
execute_process(COMMAND "${BLUR_BUNDLE_PYTHON}" "${_notice_helper}" validate-source
    --source-app "${_source}" --licenses "${BLUR_NATIVE_LICENSE_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${_stage}")
file(COPY "${_source}" DESTINATION "${_stage}")
set(_app "${_stage}/${_app_name}")
include(BundleUtilities)
set(_search_dirs "/opt/homebrew/lib;/usr/local/lib")
# Record the ORIGINAL resolved closure before fixup. BundleUtilities remains the
# only resolver/rewriter; COPYFLAG1 keys include real symlink targets added by it.
get_bundle_keys("${_app}" "" "${_search_dirs}" _keys)
set(_dependencies "${_stage}/.bundle-dependencies.tsv")
file(WRITE "${_dependencies}" "")
foreach(_key IN LISTS _keys)
    if("${${_key}_COPYFLAG}" STREQUAL "1")
        file(APPEND "${_dependencies}" "${${_key}_RESOLVED_ITEM}\t${${_key}_RESOLVED_EMBEDDED_ITEM}\n")
    endif()
endforeach()
execute_process(COMMAND "${BLUR_BUNDLE_PYTHON}" "${_notice_helper}" collect
    --source-app "${_source}" --app "${_app}" --licenses "${BLUR_NATIVE_LICENSE_DIR}"
    --dependencies "${_dependencies}" COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE "${_dependencies}")
clear_bundle_keys(_keys)
fixup_bundle("${_app}" "" "${_search_dirs}")
execute_process(COMMAND "${BLUR_BUNDLE_PYTHON}" "${_notice_helper}" inspect --app "${_app}"
    OUTPUT_VARIABLE _minimum_os OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${BLUR_BUNDLE_PYTHON}" "${_notice_helper}" metadata --app "${_app}"
    --identifier "${BLUR_BUNDLE_IDENTIFIER}" --name "${BLUR_BUNDLE_NAME}"
    --version "${BLUR_BUNDLE_VERSION}" --build-version "${BLUR_BUNDLE_BUILD_VERSION}"
    --minimum-os "${_minimum_os}" ${_game_mode_args} COMMAND_ERROR_IS_FATAL ANY)
file(GLOB_RECURSE _bundled_dylibs "${_app}/Contents/Frameworks/*.dylib")
foreach(_library IN LISTS _bundled_dylibs)
    if(NOT IS_SYMLINK "${_library}")
        execute_process(COMMAND /usr/bin/codesign --force --sign - "${_library}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
endforeach()
# Seal final nested-library hashes and notice inventory before sealing resources.
# The executable's final code signature cannot hash itself inside that inventory.
execute_process(COMMAND "${BLUR_BUNDLE_PYTHON}" "${_notice_helper}" seal --app "${_app}"
    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND /usr/bin/codesign --force --sign - "${_app}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND /usr/bin/codesign --verify --deep --strict "${_app}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${BLUR_BUNDLE_PYTHON}" "${_notice_helper}" verify --app "${_app}"
    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
message(STATUS "Staged local development app: ${_app}")
message(STATUS "Minimum bundled runtime: macOS ${_minimum_os}. No game data, notarization or release is included.")
