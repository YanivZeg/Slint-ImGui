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

CPMAddPackage(
    NAME ImGui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.92.5
)

CPMAddPackage(
    NAME ImPlot
    GITHUB_REPOSITORY epezent/implot
    GIT_TAG HEAD
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLES REQUIRED glesv2)

find_package(Slint REQUIRED)

add_library(imgui_lib STATIC)
    target_include_directories(imgui_lib PUBLIC
        ${ImGui_SOURCE_DIR}
        ${ImGui_SOURCE_DIR}/backends
    )
    target_sources(imgui_lib
        PRIVATE
            ${ImGui_SOURCE_DIR}/imgui.cpp
            ${ImGui_SOURCE_DIR}/imgui_demo.cpp
            ${ImGui_SOURCE_DIR}/imgui_draw.cpp
            ${ImGui_SOURCE_DIR}/imgui_tables.cpp
            ${ImGui_SOURCE_DIR}/imgui_widgets.cpp
            ${ImGui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_compile_definitions(imgui_lib PUBLIC
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        IMGUI_IMPL_OPENGL_ES3
    )
    target_link_libraries(imgui_lib PUBLIC
        ${GLES_LIBRARIES}
    )
    add_library(imgui::imgui ALIAS imgui_lib)

add_library(implot_lib STATIC)
    target_include_directories(implot_lib PUBLIC
        ${ImPlot_SOURCE_DIR}
    )
    target_sources(implot_lib
        PRIVATE
            ${ImPlot_SOURCE_DIR}/implot.cpp
            # ${ImPlot_SOURCE_DIR}/implot_demo.cpp
            ${ImPlot_SOURCE_DIR}/implot_items.cpp
    )
    target_link_libraries(implot_lib PUBLIC imgui::imgui)
    add_library(implot::implot ALIAS implot_lib)

add_executable(slint-imgui src/main.cpp)
target_link_libraries(slint-imgui PRIVATE Slint::Slint imgui::imgui implot::implot ${GLES_LIBRARIES})
slint_target_sources(slint-imgui src/scene.slint)
