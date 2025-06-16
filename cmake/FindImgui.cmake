# FindImGui.cmake - Locates the Dear ImGui library (used as a git submodule)

# This module defines:
#   IMGUI_FOUND
#   IMGUI_INCLUDE_DIRS
#   IMGUI_LIBRARIES

# Source files (manually listed)
set(IMGUI_SOURCES
    ${IMGUI_ROOT_DIR}/imgui.cpp
    ${IMGUI_ROOT_DIR}/imgui_demo.cpp
    ${IMGUI_ROOT_DIR}/imgui_draw.cpp
    ${IMGUI_ROOT_DIR}/imgui_tables.cpp
    ${IMGUI_ROOT_DIR}/imgui_widgets.cpp
    ${IMGUI_ROOT_DIR}/backends/imgui_impl_dx12.cpp
    ${IMGUI_ROOT_DIR}/backends/imgui_impl_win32.cpp
)

# Create static target if not already present
if(NOT TARGET imgui)
    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC
        ${IMGUI_ROOT_DIR}
        ${IMGUI_ROOT_DIR}/backends
    )
endif()

# Set variables expected by find_package consumers
set(IMGUI_INCLUDE_DIRS ${IMGUI_ROOT_DIR} ${IMGUI_ROOT_DIR}/backends)
set(IMGUI_LIBRARIES imgui)
set(IMGUI_FOUND TRUE)
