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
# GoogleTest support
# ----------------------------------------------------------------------------
option(BUILD_TESTING "Build tests" ON)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

# ----------------------------------------------------------------------------
# NVRHI (D3D12 only)
# ----------------------------------------------------------------------------
set(NVRHI_WITH_VULKAN OFF CACHE BOOL "")
set(NVRHI_WITH_DX12 ON CACHE BOOL "")
set(NVRHI_WITH_DX11 OFF CACHE BOOL "")
add_subdirectory(external/nvrhi)

# ----------------------------------------------------------------------------
# Library target for shared code
# ----------------------------------------------------------------------------
file(GLOB_RECURSE LIB_SOURCES 
    src/*.cpp 
    src/*.h
)
# Remove main.cpp from library sources
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_library(007RendererLib STATIC ${LIB_SOURCES})

target_compile_definitions(007RendererLib PUBLIC
    PROJECT_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
    PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}/src"
    PROJECT_EXTERNAL_DIR="${CMAKE_SOURCE_DIR}/external"
    PROJECT_LOG_DIR="${CMAKE_SOURCE_DIR}/logs"
)

target_include_directories(007RendererLib PUBLIC
    src/
    external/nvrhi/include
    ${SLANG_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIRS}
)

target_link_libraries(007RendererLib PUBLIC
    nvrhi
    nvrhi_d3d12
    d3d12 dxgi dxguid
    ${SLANG_LIBRARIES}
    ${IMGUI_LIBRARIES}
    spdlog::spdlog
    glm::glm
)

# ----------------------------------------------------------------------------
# Executable target
# ----------------------------------------------------------------------------
add_executable(007Renderer src/main.cpp)

target_link_libraries(007Renderer PRIVATE
    007RendererLib
)

# ----------------------------------------------------------------------------
# Debug/Release configuration macros
# ----------------------------------------------------------------------------
target_compile_definitions(007RendererLib PUBLIC
    $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
    $<$<CONFIG:Release>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
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

# ----------------------------------------------------------------------------
# GoogleTest configuration
# ----------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    
    file(GLOB_RECURSE Tests tests/*.cpp tests/*.h)
    add_executable(007RendererTests ${Tests})
    
    target_link_libraries(007RendererTests PRIVATE
        007RendererLib
        gtest_main
        gtest
    )
    
    target_include_directories(007RendererTests PRIVATE
        src/
        external/googletest/googletest/include
    )
    
    # Set output directory for test executable
    set_target_properties(007RendererTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
    )
    
    include(GoogleTest)
    gtest_discover_tests(007RendererTests)
    
    # Add custom test target that works with both single and multi-config generators
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
        DEPENDS 007RendererTests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running all tests"
    )
endif()