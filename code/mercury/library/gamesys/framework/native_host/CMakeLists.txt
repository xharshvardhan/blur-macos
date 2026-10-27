cmake_minimum_required(VERSION 3.24)
project(NativeHost VERSION 0.1.0 LANGUAGES CXX OBJCXX)
if(NOT APPLE)
    message(FATAL_ERROR "NativeHost implements the Cocoa macOS host only")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_library(NATIVE_HOST_COCOA Cocoa REQUIRED)
find_library(NATIVE_HOST_QUARTZCORE QuartzCore REQUIRED)
add_library(native_host STATIC src/Window.mm)
add_library(NativeHost::Window ALIAS native_host)
set_target_properties(native_host PROPERTIES EXPORT_NAME Window)
target_compile_features(native_host PUBLIC cxx_std_17)
set_target_properties(native_host PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES)
target_include_directories(native_host PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_options(native_host PRIVATE -fobjc-arc -Wall -Wextra)
# Export framework names, never the build machine's absolute SDK paths.
target_link_options(native_host INTERFACE "SHELL:-framework Cocoa" "SHELL:-framework QuartzCore")
install(TARGETS native_host EXPORT NativeHostTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT NativeHostTargets NAMESPACE NativeHost:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NativeHost)
configure_package_config_file(cmake/NativeHostConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NativeHostConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NativeHost)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/NativeHostConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/NativeHostConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NativeHostConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NativeHost)
install(FILES README.md PROVENANCE.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
