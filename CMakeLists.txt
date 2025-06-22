cmake_minimum_required(VERSION 3.15)
project(007Renderer)

set(CMAKE_CXX_STANDARD 17)

# Set output directories for different build types
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)

# ----------------------------------------------------------------------------
# Slang shader support
# ----------------------------------------------------------------------------
set(SLANG_ROOT_DIR "${CMAKE_SOURCE_DIR}/external/slang")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(SLANG REQUIRED COMPONENTS slang slang-rt gfx)

# ----------------------------------------------------------------------------
# ImGui support
# ----------------------------------------------------------------------------
set(IMGUI_ROOT_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
find_package(ImGui REQUIRED)

# ----------------------------------------------------------------------------
# Spdlog support
# ----------------------------------------------------------------------------
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/spdlog)

# ----------------------------------------------------------------------------
# GLM support
# ----------------------------------------------------------------------------
set(GLM_ENABLE_EXPERIMENTAL ON CACHE BOOL "Enable GLM experimental features")
set(GLM_FORCE_RADIANS ON CACHE BOOL "Use radians in GLM")
set(GLM_ROOT_DIR "${CMAKE_SOURCE_DIR}/external/glm")
add_subdirectory(${GLM_ROOT_DIR} EXCLUDE_FROM_ALL)

# ----------------------------------------------------------------------------
# NVRHI (D3D12 only)
# ----------------------------------------------------------------------------
set(NVRHI_WITH_VULKAN OFF CACHE BOOL "")
set(NVRHI_WITH_DX12 ON CACHE BOOL "")
set(NVRHI_WITH_DX11 OFF CACHE BOOL "")
add_subdirectory(external/nvrhi)

# ----------------------------------------------------------------------------
# Executable target
# ----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(007Renderer ${SOURCES})
add_compile_definitions(PROJECT_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
add_compile_definitions(PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}/src")
add_compile_definitions(PROJECT_EXTERNAL_DIR="${CMAKE_SOURCE_DIR}/external")

target_include_directories(007Renderer PRIVATE
    src/
    external/nvrhi/include
    ${SLANG_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIRS}
)

target_link_libraries(007Renderer PRIVATE
    nvrhi
    nvrhi_d3d12
    d3d12 dxgi dxguid
    ${SLANG_LIBRARIES}
    ${IMGUI_LIBRARIES}
    spdlog::spdlog
    glm::glm
)

# ----------------------------------------------------------------------------
# Debug/Release configuration macros
# ----------------------------------------------------------------------------
target_compile_definitions(007Renderer PRIVATE
    $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
    $<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
)

# ----------------------------------------------------------------------------
# Post-build: Copy Slang runtime DLLs
# ----------------------------------------------------------------------------
add_custom_command(TARGET 007Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SLANG_ROOT_DIR}/bin/slang.dll"
        "${SLANG_ROOT_DIR}/bin/gfx.dll"
        $<TARGET_FILE_DIR:007Renderer>
)

# ----------------------------------------------------------------------------
# Post-build: Copy DXC runtime DLLs
# ----------------------------------------------------------------------------
set(DXC_DLL_DIR "${CMAKE_SOURCE_DIR}/external/dxc/bin/x64")

add_custom_command(TARGET 007Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_DLL_DIR}/dxcompiler.dll"
        "${DXC_DLL_DIR}/dxil.dll"
        $<TARGET_FILE_DIR:007Renderer>
)
