cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 13.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# VCPKG INTEGRATION
# =============================================================================
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# =============================================================================
# OPTIONS
# =============================================================================
option(ENGINE_BUILD_EDITOR "Build with Editor UI" ON)
option(ENGINE_BUILD_TESTS "Build unit tests" OFF)
option(ENGINE_ENABLE_PROFILING "Enable Tracy profiling" OFF)
option(ENGINE_ENABLE_VULKAN "Enable Vulkan renderer backend" OFF)

# =============================================================================
# COMPILER FEATURE CHECKS
# =============================================================================
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("/Zc:__cplusplus" HAS_ZC_CPP)
check_cxx_compiler_flag("/permissive-" HAS_PERMISSIVE)

# =============================================================================
# SOURCE FILES — CORE ENGINE
# =============================================================================
set(CORE_SOURCES
    core/World.cpp
    core/Log.cpp
    core/GameSystems.cpp
    core/Database.cpp
)

# =============================================================================
# SOURCE FILES — MATH (AP-13)
# =============================================================================
# Header-only: Keine .cpp nötig. PCH optimiert Kompilierung.

# =============================================================================
# SOURCE FILES — MEMORY (AP-14)
# =============================================================================
set(MEMORY_SOURCES
    memory/PoolAllocator.cpp
    memory/StackAllocator.cpp
    memory/FreelistAllocator.cpp
    memory/SlabAllocator.cpp
)

# =============================================================================
# SOURCE FILES — NETWORK (AP-32–AP-36: UDP + Reliability)
# =============================================================================
set(NETWORK_SOURCES
    network/UdpSocket.cpp
    network/ReliableUdp.cpp
    network/NetworkServer.cpp
)

# =============================================================================
# SOURCE FILES — CLIENT
# =============================================================================
set(CLIENT_SOURCES
    client/Connection.cpp
    client/ClientTick.cpp
    client/Renderer.cpp
)

# =============================================================================
# SOURCE FILES — SERVER
# =============================================================================
set(SERVER_SOURCES
    server/Network.cpp
    server/Validation.cpp
    server/PacketHandler.cpp
    server/Server.cpp
)

# =============================================================================
# SOURCE FILES — EDITOR
# =============================================================================
set(EDITOR_SOURCES
    editor/CommandSystem.cpp
    editor/AssetDatabase.cpp
    editor/EntityEditor.cpp
    editor/DataDefinitionEditor.cpp
    editor/EditorPanels.cpp
    editor/EditorRuntime.cpp
)

# =============================================================================
# SOURCE FILES — VULKAN RENDERER (AP-01, AP-02, AP-03)
# =============================================================================
set(RENDERER_VULKAN_SOURCES
    renderer_vulkan/VulkanContext.cpp
    renderer_vulkan/VulkanSwapchain.cpp
    renderer_vulkan/VulkanQueue.cpp
    renderer_vulkan/VulkanDebug.cpp
    renderer_vulkan/VulkanRenderPass.cpp
    renderer_vulkan/VulkanResourceManager.cpp
)

# =============================================================================
# SOURCE FILES — ECS (AP-20: Archetype Storage — HEADER-ONLY)
# =============================================================================
# ECS is header-only in V13.1. Implementation in headers for inlining.
# No .cpp files needed for core ECS.

# =============================================================================
# MAIN
# =============================================================================
set(MAIN_SOURCE main.cpp)

# =============================================================================
# ALLE SOURCES ZUSAMMENFÜHREN
# =============================================================================
set(ALL_SOURCES
    ${CORE_SOURCES}
    ${MEMORY_SOURCES}
    ${NETWORK_SOURCES}
    ${CLIENT_SOURCES}
    ${SERVER_SOURCES}
    ${EDITOR_SOURCES}
    ${MAIN_SOURCE}
)

# Vulkan-Renderer nur bei aktiviertem Flag
if(ENGINE_ENABLE_VULKAN)
    list(APPEND ALL_SOURCES ${RENDERER_VULKAN_SOURCES})
    message(STATUS "[CMake] Vulkan renderer enabled")
else()
    message(STATUS "[CMake] Using OpenGL legacy renderer")
endif()

# ECS nur bei aktiviertem Flag (nach AP-23)
# if(ENGINE_ENABLE_ECS)
#     list(APPEND ALL_SOURCES ${ECS_SOURCES})
# endif()

# =============================================================================
# EXECUTABLE
# =============================================================================
add_executable(TheSeed ${ALL_SOURCES})

# =============================================================================
# DEPENDENCIES (via vcpkg)
# =============================================================================
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(SQLite3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

find_path(IMGUI_INCLUDE_DIR imgui.h PATH_SUFFIXES imgui)
find_library(IMGUI_LIBRARY imgui)

if(NOT IMGUI_INCLUDE_DIR OR NOT IMGUI_LIBRARY)
    message(FATAL_ERROR "ImGui not found. Install via vcpkg: imgui[glfw-binding,opengl3-binding]")
endif()

find_package(glad CONFIG REQUIRED)

if(ENGINE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

# =============================================================================
# INCLUDE DIRECTORIES
# =============================================================================
target_include_directories(TheSeed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/math
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/client
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer_vulkan
    ${IMGUI_INCLUDE_DIR}
)

# =============================================================================
# DEFINES
# =============================================================================
target_compile_definitions(TheSeed PRIVATE
    $<$<BOOL:${ENGINE_BUILD_EDITOR}>:ENGINE_BUILD_EDITOR=1>
    $<$<BOOL:${ENGINE_ENABLE_PROFILING}>:TRACY_ENABLE>
    $<$<BOOL:${ENGINE_ENABLE_VULKAN}>:ENGINE_VULKAN_BACKEND>
)

if(WIN32)
    target_compile_definitions(TheSeed PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# =============================================================================
# COMPILER FLAGS
# =============================================================================
if(MSVC)
    target_compile_options(TheSeed PRIVATE
        /W4
        $<$<BOOL:${HAS_PERMISSIVE}>:/permissive->
        $<$<BOOL:${HAS_ZC_CPP}>:/Zc:__cplusplus>
        /Zc:preprocessor
        /EHsc
        /MP
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(TheSeed PRIVATE /O2 /GL)
        target_link_options(TheSeed PRIVATE /LTCG)
    endif()
else()
    target_compile_options(TheSeed PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
    )
endif()

# =============================================================================
# LINK LIBRARIES
# =============================================================================
target_link_libraries(TheSeed PRIVATE
    OpenGL::GL
    glfw
    SQLite::SQLite3
    glm::glm
    glad::glad
    ${IMGUI_LIBRARY}
    $<$<BOOL:${ENGINE_ENABLE_VULKAN}>:Vulkan::Vulkan>
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:sqlite3>
)

# =============================================================================
# PRECOMPILED HEADER
# =============================================================================
target_precompile_headers(TheSeed PRIVATE
    "<vector>"
    "<string>"
    "<map>"
    "<algorithm>"
    "<format>"
    "<span>"
    "<ranges>"
)

# =============================================================================
# INSTALL
# =============================================================================
install(TARGETS TheSeed DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/project/ DESTINATION share/the-seed/project OPTIONAL)

# =============================================================================
# TESTS
# =============================================================================
if(ENGINE_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
