cmake_minimum_required(VERSION 3.25)
# =============================================================================
# TheSeed Engine — CMake Build Configuration (V13.2)
# =============================================================================
# KORREKTUR: Alle neuen Quelldateien hinzugefügt. C++23-Standard korrekt
# gesetzt. Abhängigkeiten über vcpkg verwaltet.
# =============================================================================

project(TheSeed VERSION 13.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Compiler-Flags
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /WX- /permissive- /Zc:__cplusplus)
    add_compile_options(/MP) # Multi-processor compilation
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-O3 -march=native)
endif()

# =============================================================================
# vcpkg Integration
# =============================================================================
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

# =============================================================================
# Abhängigkeiten finden
# =============================================================================
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Threads REQUIRED)

# =============================================================================
# Vulkan (optional)
# =============================================================================
find_package(Vulkan)
if(Vulkan_FOUND)
    add_compile_definitions(VULKAN_AVAILABLE)
endif()

# =============================================================================
# Quelldateien
# =============================================================================
set(CORE_SOURCES
    core/World.cpp
    core/GameSystems.cpp
    core/Database.cpp
    core/Log.cpp
)

set(ECS_SOURCES
    ecs/ecs_EcsWorld.cpp
)

set(NETWORK_SOURCES
    network/network_UdpSocket.cpp
    network/network_NetworkServer.cpp
    network/network_ReliableUdp.cpp
)

set(SERVER_SOURCES
    server/Network.cpp
    server/PacketHandler.cpp
    server/Validation.cpp
    server/ThreadPool.cpp
    server/Server.cpp
)

set(CLIENT_SOURCES
    client/Connection.cpp
    client/ClientTick.cpp
    client/Renderer.cpp
    client/Interpolation.cpp
)

set(EDITOR_SOURCES
    editor/EditorRuntime.cpp
)

set(VULKAN_SOURCES
    renderer_vulkan/VulkanRenderer.cpp
    renderer_vulkan/VulkanContext.cpp
    renderer_vulkan/VulkanSwapchain.cpp
    renderer_vulkan/VulkanQueue.cpp
    renderer_vulkan/VulkanDebug.cpp
    renderer_vulkan/VulkanRenderPass.cpp
    renderer_vulkan/VulkanResourceManager.cpp
    renderer_vulkan/Mesh.cpp
    renderer_vulkan/Texture.cpp
    renderer_vulkan/UniformBuffer.cpp
    renderer_vulkan/ShadowMap.cpp
)

set(MATH_SOURCES
    math/Vector.cpp
    math/Matrix.cpp
    math/Quaternion.cpp
    math/Frustum.cpp
)

set(MEMORY_SOURCES
    memory/PoolAllocator.cpp
    memory/StackAllocator.cpp
    memory/FreelistAllocator.cpp
    memory/SlabAllocator.cpp
)

set(ALL_SOURCES
    main.cpp
    ${CORE_SOURCES}
    ${ECS_SOURCES}
    ${NETWORK_SOURCES}
    ${SERVER_SOURCES}
    ${CLIENT_SOURCES}
    ${EDITOR_SOURCES}
    ${VULKAN_SOURCES}
    ${MATH_SOURCES}
    ${MEMORY_SOURCES}
)

# =============================================================================
# Executable
# =============================================================================
add_executable(TheSeed ${ALL_SOURCES})

# =============================================================================
# Include-Verzeichnisse
# =============================================================================
target_include_directories(TheSeed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/client
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer_vulkan
    ${CMAKE_CURRENT_SOURCE_DIR}/math
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
)

# =============================================================================
# Link-Bibliotheken
# =============================================================================
target_link_libraries(TheSeed PRIVATE
    glfw
    imgui::imgui
    GLEW::GLEW
    OpenGL::GL
    SQLite::SQLite3
    magic_enum::magic_enum
    Threads::Threads
)

if(Vulkan_FOUND)
    target_link_libraries(TheSeed PRIVATE Vulkan::Vulkan)
endif()

# =============================================================================
# Windows-spezifische Bibliotheken
# =============================================================================
if(WIN32)
    target_link_libraries(TheSeed PRIVATE
        ws2_32      # Winsock2
        iphlpapi    # IP Helper
    )
endif()

# =============================================================================
# Editor-Build
# =============================================================================
option(ENGINE_BUILD_EDITOR "Build with Editor support" ON)
if(ENGINE_BUILD_EDITOR)
    target_compile_definitions(TheSeed PRIVATE ENGINE_BUILD_EDITOR=1)
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS TheSeed DESTINATION bin)
install(DIRECTORY data/ DESTINATION bin/data)
