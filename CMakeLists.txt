cmake_minimum_required(VERSION 3.22)
project(Arke-Android VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler options
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -O3)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Core sources
set(ARKE_CORE_SOURCES
    src/core/config.cpp
    src/core/logger.cpp
    src/core/uniform_bridge.cpp
    src/shader/properties_parser.cpp
    src/shader/preprocessor.cpp
    src/shader/compiler.cpp
    src/shader/pipeline.cpp
)

# Android platform sources
set(ARKE_ANDROID_SOURCES
    src/platform/android/jni_entry.cpp
    src/platform/android/dobby_hook_layer.cpp
    src/platform/android/gles_render_hook.cpp
    src/platform/android/vulkan_render_hook.cpp
    src/platform/android/imgui_android_overlay.cpp
    third_party/dobby/dobby.c
    third_party/shadowhook/shadowhook.c
)

# Build libarke_android.so shared library
add_library(arke_android SHARED
    ${ARKE_CORE_SOURCES}
    ${ARKE_ANDROID_SOURCES}
)

target_include_directories(arke_android PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
)

target_link_libraries(arke_android PRIVATE
    log
    dl
)

if (ANDROID)
    target_link_libraries(arke_android PRIVATE
        android
        EGL
        GLESv3
    )
endif()

# Unit tests executable
add_executable(test_shader_transpile
    tests/test_shader_transpile.cpp
    ${ARKE_CORE_SOURCES}
)

target_include_directories(test_shader_transpile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(test_shader_transpile PRIVATE
    log
    dl
)
