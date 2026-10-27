# [Blur2PC] Pinned public MojoShader decoder/Metal translator. Keep its source
# pristine; only compile the pure translation library, without graphics glue.
include(FetchContent)
FetchContent_Declare(BlurMojoShader
    URL https://codeload.github.com/icculus/mojoshader/tar.gz/ad5dff84830c2863c841f4b1f4e3df78c705b383
    URL_HASH SHA256=728ff9098115059c4012895b7a85065688c857fdadf9fbc04484fec8c7be3081
    DOWNLOAD_DIR "${BLUR_TOP}/_work/vendor"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    SOURCE_SUBDIR blur-native-translation-only)
FetchContent_MakeAvailable(BlurMojoShader)
# [Blur2PC] Preserve the pinned decoder while correcting its lost TEXLDL operand
# in a checked generated copy. Never mutate the fetched vendor source.
set(_blur_mojo_metal_profile "${CMAKE_CURRENT_BINARY_DIR}/generated/mojoshader_profile_metal.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${BLUR_TOP}/tools/macos/patch_mojoshader.py
    ${blurmojoshader_SOURCE_DIR}/profiles/mojoshader_profile_metal.c)
execute_process(COMMAND ${Python3_EXECUTABLE} ${BLUR_TOP}/tools/macos/patch_mojoshader.py
    ${blurmojoshader_SOURCE_DIR}/profiles/mojoshader_profile_metal.c ${_blur_mojo_metal_profile}
    RESULT_VARIABLE _blur_mojo_patch_result)
if(NOT _blur_mojo_patch_result EQUAL 0)
    message(FATAL_ERROR "Pinned MojoShader Metal TEXLDL patch failed")
endif()
add_library(blur_mojoshader STATIC
    ${blurmojoshader_SOURCE_DIR}/mojoshader.c
    ${blurmojoshader_SOURCE_DIR}/mojoshader_common.c
    ${blurmojoshader_SOURCE_DIR}/profiles/mojoshader_profile_common.c
    ${_blur_mojo_metal_profile})
target_include_directories(blur_mojoshader PUBLIC ${blurmojoshader_SOURCE_DIR}
    PRIVATE ${blurmojoshader_SOURCE_DIR}/profiles)
target_compile_definitions(blur_mojoshader PUBLIC MOJOSHADER_NO_VERSION_INCLUDE)
target_compile_definitions(blur_mojoshader PRIVATE
    SUPPORT_PROFILE_D3D=0 SUPPORT_PROFILE_BYTECODE=0 SUPPORT_PROFILE_HLSL=0
    SUPPORT_PROFILE_GLSL=0 SUPPORT_PROFILE_GLSL120=0 SUPPORT_PROFILE_GLSLES=0
    SUPPORT_PROFILE_GLSLES3=0 SUPPORT_PROFILE_ARB1=0 SUPPORT_PROFILE_ARB1_NV=0
    SUPPORT_PROFILE_SPIRV=0 SUPPORT_PROFILE_GLSPIRV=0 SUPPORT_PROFILE_METAL=1)
file(MAKE_DIRECTORY "${BLUR_MAC_RUN_DIR}/licenses")
configure_file("${blurmojoshader_SOURCE_DIR}/LICENSE.txt" "${BLUR_MAC_RUN_DIR}/licenses/MojoShader.txt" COPYONLY)
