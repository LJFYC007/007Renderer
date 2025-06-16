cmake_minimum_required(VERSION 3.15)
project(007Renderer)

set(CMAKE_CXX_STANDARD 17)

# ----------------------------------------------------------------------------
# Slang Shader Compilation
# ----------------------------------------------------------------------------

set(SLANG_ROOT_DIR "${CMAKE_SOURCE_DIR}/external/slang")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(SLANG REQUIRED COMPONENTS slang slang-rt gfx)

# ----------------------------------------------------------------------------
# Add NVRHI
# ----------------------------------------------------------------------------

set(NVRHI_WITH_VULKAN OFF CACHE BOOL "")
set(NVRHI_WITH_DX12 ON CACHE BOOL "")
set(NVRHI_WITH_DX11 OFF CACHE BOOL "")

add_subdirectory(external/nvrhi)

# ----------------------------------------------------------------------------
# Your Executable
# ----------------------------------------------------------------------------

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(007Renderer ${SOURCES})

target_include_directories(007Renderer PRIVATE
    external/nvrhi/include
    ${SLANG_INCLUDE_DIRS}
)

target_link_libraries(007Renderer PRIVATE
    nvrhi
    nvrhi_d3d12
    d3d12 dxgi dxguid
    ${SLANG_LIBRARIES}
)

add_custom_command(TARGET 007Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SLANG_ROOT_DIR}/bin/slang.dll"
        "${SLANG_ROOT_DIR}/bin/gfx.dll"
        $<TARGET_FILE_DIR:007Renderer>
)

# ----------------------------------------------------------------------------
# Copy DXC runtime DLLs (dxcompiler.dll, dxil.dll)
# ----------------------------------------------------------------------------
set(DXC_DLL_DIR "${CMAKE_SOURCE_DIR}/external/dxc/bin/x64")

add_custom_command(TARGET 007Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_DLL_DIR}/dxcompiler.dll"
        "${DXC_DLL_DIR}/dxil.dll"
        $<TARGET_FILE_DIR:007Renderer>
)
