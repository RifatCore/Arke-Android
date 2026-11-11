cmake_minimum_required(VERSION 3.22)
project(Arke-Android VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler options
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -O3)
endif()

add_compile_definitions(IMGUI_IMPL_OPENGL_ES3)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SPIRV-Cross
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
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
    third_party/SPIRV-Cross/spirv_cross.cpp
    third_party/SPIRV-Cross/spirv_parser.cpp
    third_party/SPIRV-Cross/spirv_cross_parsed_ir.cpp
    third_party/SPIRV-Cross/spirv_glsl.cpp
    third_party/SPIRV-Cross/spirv_cfg.cpp
)

# Android platform sources
set(ARKE_ANDROID_SOURCES
    src/platform/android/jni_entry.cpp
    src/platform/android/bionic_compat.c
    src/platform/android/dobby_hook_layer.cpp
    src/platform/android/gles_render_hook.cpp
    src/platform/android/vulkan_render_hook.cpp
    src/platform/android/imgui_android_overlay.cpp
    third_party/dobby/dobby.c
    third_party/shadowhook/shadowhook.c
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_android.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

# Build libarke_android.so shared library
add_library(arke_android SHARED
    ${ARKE_CORE_SOURCES}
    ${ARKE_ANDROID_SOURCES}
)

target_include_directories(arke_android PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/SPIRV-Cross>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui>
)

target_link_libraries(arke_android PRIVATE
    log
    dl
    EGL
    GLESv3
)

target_link_options(arke_android PRIVATE
    -Wl,-soname,libarke_android.so
    -Wl,-z,max-page-size=16384
)

if (ANDROID)
    target_link_libraries(arke_android PRIVATE
        android
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
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SPIRV-Cross
)

target_link_libraries(test_shader_transpile PRIVATE
    log
    dl
)
