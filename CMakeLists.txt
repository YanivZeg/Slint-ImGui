cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_C_COMPILER /opt/llvm-20.1.7/bin/clang)
set(CMAKE_CXX_COMPILER /opt/llvm-20.1.7/bin/clang++)
# ---- Project ----

# Note: update this to your new project's name and version
project(
  SlintImGui
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake_modules")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options ----

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ---- Sanitizer Options ----
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)
option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(ENABLE_MSAN "Enable Memory Sanitizer" OFF)

# Sanitizer configuration
if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN OR ENABLE_MSAN)
    # Ensure we're using Clang for sanitizers
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Sanitizers require Clang compiler")
    endif()

    # Set build type to Debug for better sanitizer output

    # Common sanitizer flags
    set(SANITIZER_FLAGS "")
    list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer")
    list(APPEND SANITIZER_FLAGS "-g")
    list(APPEND SANITIZER_FLAGS "-O0")

    if(ENABLE_ASAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=address,undefined")
        message(STATUS "Address Sanitizer enabled")
    endif()

    if(ENABLE_TSAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=thread,undefined")
        message(STATUS "Thread Sanitizer enabled")
    endif()

    if(ENABLE_MSAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=memory,undefined")
        message(STATUS "Memory Sanitizer enabled")
    endif()

    # Apply sanitizer flags
    add_compile_options(${SANITIZER_FLAGS})
    add_link_options(${SANITIZER_FLAGS})

    # Set environment variables for better sanitizer output
    set(ENV{ASAN_OPTIONS} "detect_leaks=1:abort_on_error=0:symbolize=1:external_symbolizer_path=/opt/llvm-20.1.7/bin/llvm-symbolizer:verbosity=1:print_stats=1:strip=0")
    set(ENV{TSAN_OPTIONS} "halt_on_error=0:second_deadlock_stack=1:print_signal_events=1:print_benign=1:print_suppressions=1:report_signal_unsafe=1:report_thread_leaks=1:history_size=7:external_symbolizer_path=/usr/lib/llvm-19/bin/llvm-symbolizer:symbolize=1:verbosity=2:report_atomic_races=1")
    set(ENV{UBSAN_OPTIONS} "print_stacktrace=1:abort_on_error=0")
    set(ENV{MSAN_OPTIONS} "abort_on_error=0")
endif()

# Set Clang module flags
# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     set(CLANG_MODULE_CACHE_DIR "${CMAKE_BINARY_DIR}/clang-module-cache")
#     add_compile_options(-fmodules -fbuiltin-module-map)
#     add_compile_options(-fimplicit-modules -fimplicit-module-maps)
#     add_compile_options(-fprebuilt-module-path=${CMAKE_CXX_MODULE_DIRECTORY})
#     add_compile_options(-fmodules-cache-path=${CLANG_MODULE_CACHE_DIR})
#     add_compile_options(-stdlib=libc++)
# endif()
# # Set GCC module flags
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#     add_compile_options(-fmodules-ts)
# endif()

# Warnings and errors flags
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)

find_package(Slint REQUIRED)

add_library(imgui_module)
target_compile_features(imgui_module PUBLIC cxx_std_23)
target_include_directories(imgui_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/backends
)
target_sources(imgui_module
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/imgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/imgui_demo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/imgui_draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/imgui_tables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/imgui_widgets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/backends/imgui_impl_opengl3.cpp
)
# PUBLIC so any code that includes imgui.h (e.g. main.cpp) uses the same
# layout as the compiled imgui library; otherwise DebugCheckVersionAndDataLayout fails.
target_compile_definitions(imgui_module PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_IMPL_OPENGL_ES3
)
target_link_libraries(imgui_module PUBLIC
    ${GLES_LIBRARIES}
    # glfw
    # Vulkan::module
    # $<$<PLATFORM_ID:Linux>:-lX11> # Starting from ImGui v1.92.3, using GLFW backend on Linux/BSD etc. requires linking with -lX11.
)

add_library(imgui::module ALIAS imgui_module)

add_executable(slint-imgui src/main.cpp)
target_link_libraries(slint-imgui PRIVATE Slint::Slint imgui::module ${GLES_LIBRARIES})
slint_target_sources(slint-imgui src/scene.slint)
