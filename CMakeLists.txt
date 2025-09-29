cmake_minimum_required(VERSION 3.15)
project(007Renderer)

set(ENV{VSLANG} 1033) # Set Visual Studio language to English
set(CMAKE_CXX_STANDARD 17)

# Enable project folder organization in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set output directories for different build types
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo)

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
set_target_properties(imgui PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# ImGui node editor support
# ----------------------------------------------------------------------------
set(IMGUI_NODE_EDITOR_DIR "${CMAKE_SOURCE_DIR}/external/imgui-node-editor")
add_library(imgui_node_editor STATIC
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor_api.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_canvas.cpp
    ${IMGUI_NODE_EDITOR_DIR}/crude_json.cpp
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor.h
    ${IMGUI_NODE_EDITOR_DIR}/imgui_node_editor_internal.h
    ${IMGUI_NODE_EDITOR_DIR}/imgui_canvas.h
    ${IMGUI_NODE_EDITOR_DIR}/crude_json.h
    ${IMGUI_NODE_EDITOR_DIR}/imgui_bezier_math.h
    ${IMGUI_NODE_EDITOR_DIR}/imgui_extra_math.h
)

target_include_directories(imgui_node_editor PUBLIC
    ${IMGUI_NODE_EDITOR_DIR}
    ${IMGUI_INCLUDE_DIRS}
)

target_link_libraries(imgui_node_editor PUBLIC imgui)
set_target_properties(imgui_node_editor PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# Spdlog support
# ----------------------------------------------------------------------------
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/spdlog)
set_target_properties(spdlog PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# TinyUSDZ support
# ----------------------------------------------------------------------------
add_subdirectory(external/tinyusdz)

# ----------------------------------------------------------------------------
# Assimp support
# ----------------------------------------------------------------------------
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
add_subdirectory(external/assimp)
set_target_properties(assimp PROPERTIES FOLDER "External")

# Hide additional Assimp targets in External folder
if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER "External")
endif()
if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES FOLDER "External")
endif()
if(TARGET uninstall)
    set_target_properties(uninstall PROPERTIES FOLDER "External")
endif()

# ----------------------------------------------------------------------------
# TinyEXR support
# ----------------------------------------------------------------------------
set(TINYEXR_DIR "${CMAKE_SOURCE_DIR}/external/tinyexr")
add_library(tinyexr INTERFACE)
target_include_directories(tinyexr INTERFACE ${TINYEXR_DIR})
# Use miniz for compression instead of system zlib to avoid dependency issues
option(USE_TINYEXR_MINIZ "Use bundled miniz with TinyEXR" ON)
add_library(miniz STATIC ${TINYEXR_DIR}/miniz.c)
target_include_directories(miniz PUBLIC ${TINYEXR_DIR})

set_target_properties(tinyexr PROPERTIES FOLDER "External")
set_target_properties(miniz PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# GLM support
# ----------------------------------------------------------------------------
set(GLM_ENABLE_EXPERIMENTAL ON CACHE BOOL "Enable GLM experimental features")
set(GLM_FORCE_RADIANS ON CACHE BOOL "Use radians in GLM")
set(GLM_ROOT_DIR "${CMAKE_SOURCE_DIR}/external/glm")
add_subdirectory(${GLM_ROOT_DIR} EXCLUDE_FROM_ALL)

# Hide GLM target in External folder
set_target_properties(glm PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# GoogleTest support
# ----------------------------------------------------------------------------
option(BUILD_TESTING "Build tests" ON)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

# Hide GoogleTest targets in External folder
set_target_properties(gtest PROPERTIES FOLDER "External")
set_target_properties(gtest_main PROPERTIES FOLDER "External")
set_target_properties(gmock PROPERTIES FOLDER "External")
set_target_properties(gmock_main PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# NVRHI (D3D12 only)
# ----------------------------------------------------------------------------
set(NVRHI_WITH_VULKAN OFF CACHE BOOL "")
set(NVRHI_WITH_DX12 ON CACHE BOOL "")
set(NVRHI_WITH_DX11 OFF CACHE BOOL "")
add_subdirectory(external/nvrhi)

# Hide NVRHI targets in External folder
set_target_properties(nvrhi PROPERTIES FOLDER "External")
set_target_properties(nvrhi_d3d12 PROPERTIES FOLDER "External")
set_target_properties(DirectX-Headers PROPERTIES FOLDER "External")
set_target_properties(DirectX-Guids PROPERTIES FOLDER "External")

# ----------------------------------------------------------------------------
# Library target for shared code
# ----------------------------------------------------------------------------
file(GLOB_RECURSE LIB_SOURCES 
    src/*.cpp 
    src/*.h
)
# Remove main.cpp from library sources
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_library(007Core STATIC ${LIB_SOURCES})

target_compile_definitions(007Core PUBLIC
    PROJECT_DIR="${CMAKE_SOURCE_DIR}"
    PROJECT_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
    PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}/src"
    PROJECT_EXTERNAL_DIR="${CMAKE_SOURCE_DIR}/external"
    PROJECT_LOG_DIR="${CMAKE_SOURCE_DIR}/logs"
)

target_include_directories(007Core PUBLIC
    src/
    external/nvrhi/include
    external/glm
    ${SLANG_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIRS}
    external/imgui-node-editor
    external/tinyusdz/src
)

target_link_libraries(007Core PUBLIC
    nvrhi
    nvrhi_d3d12
    d3d12 dxgi dxguid
    ${SLANG_LIBRARIES}
    ${IMGUI_LIBRARIES}
    imgui_node_editor
    spdlog::spdlog
    assimp
    miniz
    tinyexr
    tinyusdz::tinyusdz_static
)

# ----------------------------------------------------------------------------
# Executable target
# ----------------------------------------------------------------------------
add_executable(007Renderer src/main.cpp)

target_link_libraries(007Renderer PRIVATE
    007Core
)

# ----------------------------------------------------------------------------
# Debug/Release configuration macros
# ----------------------------------------------------------------------------
target_compile_definitions(007Core PUBLIC
    $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    TINYEXR_USE_MINIZ=1
)

# ----------------------------------------------------------------------------
# GoogleTest configuration
# ----------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    
    file(GLOB_RECURSE Tests tests/*.cpp tests/*.h)
    add_executable(007Tests ${Tests})
    
    target_link_libraries(007Tests PRIVATE
        007Core
        gtest_main
        gtest
    )
    
    target_include_directories(007Tests PRIVATE
        src/
        external/googletest/googletest/include
    )
    
    # Set output directory for test executable
    set_target_properties(007Tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo
    )
    include(GoogleTest)
    # Use PRE_TEST discovery mode to avoid running executable during configure
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
        gtest_discover_tests(007Tests DISCOVERY_MODE PRE_TEST)
    else()
        gtest_discover_tests(007Tests)
    endif()

    # Add custom test target that works with both single and multi-config generators
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
        DEPENDS 007Tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running all tests"
    )

    # Hide auxiliary test target
    set_target_properties(run_tests PROPERTIES FOLDER "Utilities")
endif()

# ----------------------------------------------------------------------------
# Hide CMake auxiliary targets in CMake folder
# ----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

# ----------------------------------------------------------------------------
# Post-build: Copy Slang runtime DLLs
# ----------------------------------------------------------------------------
add_custom_command(TARGET 007Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SLANG_ROOT_DIR}/bin/slang.dll"
        "${SLANG_ROOT_DIR}/bin/gfx.dll"
        $<TARGET_FILE_DIR:007Renderer>
)

# Copy Slang DLLs for test executable
if(BUILD_TESTING)
    add_custom_command(TARGET 007Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SLANG_ROOT_DIR}/bin/slang.dll"
            "${SLANG_ROOT_DIR}/bin/gfx.dll"
            $<TARGET_FILE_DIR:007Tests>
    )
endif()

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

# Copy DXC DLLs for test executable
if(BUILD_TESTING)
    add_custom_command(TARGET 007Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXC_DLL_DIR}/dxcompiler.dll"
            "${DXC_DLL_DIR}/dxil.dll"
            $<TARGET_FILE_DIR:007Tests>
    )
endif()
