# =============================================================================
# CMakeLists.txt — TheSeed Engine V13.2 (P3-FIX + AP-80)
# =============================================================================
# KORREKTUR P3: Alle Quelldateien und Include-Pfade vollstaendig.
# KORREKTUR AP-80: Memory Profiler Quelldateien hinzugefuegt.
# =============================================================================
cmake_minimum_required(VERSION 3.25)

project(TheSeed VERSION 13.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Quelldateien
# =============================================================================
set(SOURCES
    main.cpp
    core/World.cpp
    core/GameSystems.cpp
    core/Database.cpp
    core/Log.cpp
    ecs/ecs_EcsWorld.cpp
    network/network_UdpSocket.cpp
    network/network_NetworkServer.cpp
    network/network_ReliableUdp.cpp
    server/Network.cpp
    server/PacketHandler.cpp
    server/Validation.cpp
    server/ThreadPool.cpp
    server/Server.cpp
    server/ai/AIBehavior.cpp
    server/ai/AIParallelSystem.cpp
    server/ai/Pathfinding.cpp
    server/ai/NavMesh.cpp
    server/auth/AuthService.cpp
    server/auth/SqliteUserRepository.cpp
    server/combat/CombatSystem.cpp
    server/network/Snapshot.cpp
    client/Connection.cpp
    client/ClientTick.cpp
    client/Renderer.cpp
    client/Interpolation.cpp
    client/Animation.cpp
    editor/EditorRuntime.cpp
    editor/EditorPanels.cpp
    editor/AssetDatabase.cpp
    editor/CommandSystem.cpp
    editor/DataDefinitionEditor.cpp
    editor/EntityEditor.cpp
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
    math/Vector.cpp
    math/Matrix.cpp
    math/Quaternion.cpp
    math/Frustum.cpp
    memory/PoolAllocator.cpp
    memory/StackAllocator.cpp
    memory/FreelistAllocator.cpp
    memory/SlabAllocator.cpp
    memory/MemoryProfiler.cpp
)

# =============================================================================
# Executable
# =============================================================================
add_executable(TheSeed ${SOURCES})

# =============================================================================
# Include-Verzeichnisse
# =============================================================================
target_include_directories(TheSeed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/server
    ${CMAKE_CURRENT_SOURCE_DIR}/server/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/server/auth
    ${CMAKE_CURRENT_SOURCE_DIR}/server/combat
    ${CMAKE_CURRENT_SOURCE_DIR}/server/network
    ${CMAKE_CURRENT_SOURCE_DIR}/client
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer_vulkan
    ${CMAKE_CURRENT_SOURCE_DIR}/math
    ${CMAKE_CURRENT_SOURCE_DIR}/memory
)

# =============================================================================
# Abhaengigkeiten
# =============================================================================
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG)

if(nlohmann_json_FOUND)
    target_compile_definitions(TheSeed PRIVATE HAS_NLOHMANN_JSON)
endif()

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM libsodium)
    if(SODIUM_FOUND)
        target_compile_definitions(TheSeed PRIVATE HAS_LIBSODIUM)
        target_include_directories(TheSeed PRIVATE ${SODIUM_INCLUDE_DIRS})
    endif()
endif()

find_package(OpenSSL)
if(OpenSSL_FOUND)
    target_compile_definitions(TheSeed PRIVATE HAS_OPENSSL)
endif()

# PostgreSQL (optional)
find_package(PostgreSQL)
if(PostgreSQL_FOUND)
    target_compile_definitions(TheSeed PRIVATE HAS_POSTGRESQL)
    target_include_directories(TheSeed PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(TheSeed PRIVATE ${PostgreSQL_LIBRARIES})
endif()

# Redis (optional)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(HIREDIS hiredis)
    if(HIREDIS_FOUND)
        target_compile_definitions(TheSeed PRIVATE HAS_HIREDIS)
        target_include_directories(TheSeed PRIVATE ${HIREDIS_INCLUDE_DIRS})
        target_link_libraries(TheSeed PRIVATE ${HIREDIS_LIBRARIES})
    endif()
endif()

# Vulkan (optional)
find_package(Vulkan)
if(Vulkan_FOUND)
    target_compile_definitions(TheSeed PRIVATE VULKAN_AVAILABLE)
    target_link_libraries(TheSeed PRIVATE Vulkan::Vulkan)
endif()

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

if(nlohmann_json_FOUND)
    target_link_libraries(TheSeed PRIVATE nlohmann_json::nlohmann_json)
endif()

if(SODIUM_FOUND)
    target_link_libraries(TheSeed PRIVATE ${SODIUM_LIBRARIES})
endif()

if(OpenSSL_FOUND)
    target_link_libraries(TheSeed PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(WIN32)
    target_link_libraries(TheSeed PRIVATE ws2_32 iphlpapi)
endif()

# =============================================================================
# Compiler-Warnungen
# =============================================================================
if(MSVC)
    target_compile_options(TheSeed PRIVATE /W4 /permissive-)
else()
    target_compile_options(TheSeed PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Tests
# =============================================================================
enable_testing()
add_subdirectory(tests)
