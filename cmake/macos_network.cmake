# [Blur2PC] Original Unix Demonware protocol/transport modules with native maintained dependencies.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(CURL REQUIRED)
find_path(BLUR_TOMCRYPT_INCLUDE_DIR tomcrypt.h HINTS /opt/homebrew/include /opt/homebrew/opt/libtomcrypt/include REQUIRED)
find_library(BLUR_TOMCRYPT_LIBRARY tomcrypt HINTS /opt/homebrew/lib /opt/homebrew/opt/libtomcrypt/lib REQUIRED)
find_library(BLUR_TOMMATH_LIBRARY tommath HINTS /opt/homebrew/lib /opt/homebrew/opt/libtommath/lib REQUIRED)
set(_blur_dw_sources "${CMAKE_CURRENT_BINARY_DIR}/network_vendor_sources.cmake")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${BLUR_TOP}/tools/macos/network_vendor_sources.py"
    --root "${BLUR_TOP}" --cmake "${_blur_dw_sources}" COMMAND_ERROR_IS_FATAL ANY)
include("${_blur_dw_sources}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${BLUR_TOP}/tools/macos/network_vendor_sources.py" ${BLUR_MAC_DEMONWARE_PROJECTS})
add_library(blur_mac_demonware STATIC ${BLUR_MAC_DEMONWARE_SOURCES})
target_include_directories(blur_mac_demonware PUBLIC
    "${BLUR_TOP}/code/vendor/demonware/2.0" "${BLUR_TOMCRYPT_INCLUDE_DIR}")
target_compile_features(blur_mac_demonware PUBLIC cxx_std_17)
target_compile_options(blur_mac_demonware PRIVATE -fms-extensions -Wno-deprecated-declarations -Wno-nonportable-include-path)
target_link_libraries(blur_mac_demonware PUBLIC CURL::libcurl "${BLUR_TOMCRYPT_LIBRARY}" "${BLUR_TOMMATH_LIBRARY}")
option(BLUR_MAC_NETWORK_BUILD_TESTS "Build authored original Demonware native runtime checks" OFF)
if(BLUR_MAC_NETWORK_BUILD_TESTS)
    add_executable(blur_mac_network_vendor_test "${BLUR_TOP}/tools/macos/network_vendor_test.cpp")
    target_link_libraries(blur_mac_network_vendor_test PRIVATE blur_mac_demonware)
    add_test(NAME blur_mac_network_vendor COMMAND blur_mac_network_vendor_test)
endif()
