# [Blur2PC][macOS] Reuse upstream platform code; pin the official source archive for reproducible builds.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "Build native dependencies into the application" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build the native SDL library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build SDL's test support library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL's standalone tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL's standalone examples" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Dependencies stay in this worktree" FORCE)
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_DIR "${BLUR_TOP}/_work/vendor"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(SDL3)
file(MAKE_DIRECTORY "${BLUR_MAC_RUN_DIR}/licenses")
configure_file("${sdl3_SOURCE_DIR}/LICENSE.txt" "${BLUR_MAC_RUN_DIR}/licenses/SDL3.txt" COPYONLY)

# Reuse the public maintained SoundTouch implementation for native pitch shifting.
set(INTEGER_SAMPLES OFF CACHE BOOL "Engine DSP uses float32 samples" FORCE)
set(SOUNDSTRETCH OFF CACHE BOOL "Build only the native DSP dependency" FORCE)
set(SOUNDTOUCH_DLL OFF CACHE BOOL "Use the C++ SoundTouch interface" FORCE)
set(OPENMP OFF CACHE BOOL "Audio scheduling remains owned by the engine" FORCE)
FetchContent_Declare(SoundTouch
    URL https://www.surina.net/soundtouch/soundtouch-2.4.0.tar.gz
    URL_HASH SHA256=3dda3c9ab1e287f15028c010a66ab7145fa855dfa62763538f341e70b4d10abd
    DOWNLOAD_DIR "${BLUR_TOP}/_work/vendor"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(SoundTouch)
configure_file("${soundtouch_SOURCE_DIR}/COPYING.TXT" "${BLUR_MAC_RUN_DIR}/licenses/SoundTouch.txt" COPYONLY)
