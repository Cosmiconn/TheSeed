# =============================================================================
# tests/CMakeLists.txt — Test Build Configuration (Changelog_0012)
# =============================================================================
# Erstellt ein separates Test-Executable mit allen Unit-Tests,
# Integrationstests und Memory Profiler Tests.
# =============================================================================
cmake_minimum_required(VERSION 3.25)

project(TheSeedTests VERSION 13.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Test-Quelldateien
# =============================================================================
set(TEST_SOURCES
    TestMain.cpp
    Test_ECS.cpp
    Test_Network.cpp
    Test_Math.cpp
    Test_Interpolation.cpp
    Test_Integration.cpp
    Test_MemoryProfiler.cpp
)

set(ENGINE_SOURCES
    ../core/World.cpp
    ../core/GameSystems.cpp
    ../core/Database.cpp
    ../core/Log.cpp
    ../ecs/ecs_EcsWorld.cpp
    ../network/network_UdpSocket.cpp
    ../network/network_NetworkServer.cpp
    ../network/network_ReliableUdp.cpp
    ../server/Network.cpp
    ../server/PacketHandler.cpp
    ../server/Validation.cpp
    ../server/ThreadPool.cpp
    ../server/Server.cpp
    ../server/ai/AIBehavior.cpp
    ../server/ai/AIParallelSystem.cpp
    ../server/ai/Pathfinding.cpp
    ../server/ai/NavMesh.cpp
    ../server/auth/AuthService.cpp
    ../server/auth/SqliteUserRepository.cpp
    ../server/combat/CombatSystem.cpp
    ../server/network/Snapshot.cpp
    ../client/Connection.cpp
    ../client/ClientTick.cpp
    ../client/Renderer.cpp
    ../client/Interpolation.cpp
    ../client/Animation.cpp
    ../editor/EditorRuntime.cpp
    ../editor/EditorPanels.cpp
    ../editor/AssetDatabase.cpp
    ../editor/CommandSystem.cpp
    ../editor/DataDefinitionEditor.cpp
    ../editor/EntityEditor.cpp
    ../renderer_vulkan/VulkanRenderer.cpp
    ../renderer_vulkan/VulkanContext.cpp
    ../renderer_vulkan/VulkanSwapchain.cpp
    ../renderer_vulkan/VulkanQueue.cpp
    ../renderer_vulkan/VulkanDebug.cpp
    ../renderer_vulkan/VulkanRenderPass.cpp
    ../renderer_vulkan/VulkanResourceManager.cpp
    ../renderer_vulkan/Mesh.cpp
    ../renderer_vulkan/Texture.cpp
    ../renderer_vulkan/UniformBuffer.cpp
    ../renderer_vulkan/ShadowMap.cpp
    ../math/Vector.cpp
    ../math/Matrix.cpp
    ../math/Quaternion.cpp
    ../math/Frustum.cpp
    ../memory/PoolAllocator.cpp
    ../memory/StackAllocator.cpp
    ../memory/FreelistAllocator.cpp
    ../memory/SlabAllocator.cpp
    ../memory/MemoryProfiler.cpp
)

# =============================================================================
# Test-Executable
# =============================================================================
add_executable(TheSeedTests ${TEST_SOURCES} ${ENGINE_SOURCES})

# =============================================================================
# Include-Verzeichnisse
# =============================================================================
target_include_directories(TheSeedTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/../network
    ${CMAKE_CURRENT_SOURCE_DIR}/../server
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/auth
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/combat
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../client
    ${CMAKE_CURRENT_SOURCE_DIR}/../editor
    ${CMAKE_CURRENT_SOURCE_DIR}/../renderer_vulkan
    ${CMAKE_CURRENT_SOURCE_DIR}/../math
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory
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
    target_compile_definitions(TheSeedTests PRIVATE HAS_NLOHMANN_JSON)
endif()

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM libsodium)
    if(SODIUM_FOUND)
        target_compile_definitions(TheSeedTests PRIVATE HAS_LIBSODIUM)
        target_include_directories(TheSeedTests PRIVATE ${SODIUM_INCLUDE_DIRS})
    endif()
endif()

find_package(OpenSSL)
if(OpenSSL_FOUND)
    target_compile_definitions(TheSeedTests PRIVATE HAS_OPENSSL)
endif()

find_package(Vulkan)
if(Vulkan_FOUND)
    target_compile_definitions(TheSeedTests PRIVATE VULKAN_AVAILABLE)
    target_link_libraries(TheSeedTests PRIVATE Vulkan::Vulkan)
endif()

# =============================================================================
# Link-Bibliotheken
# =============================================================================
target_link_libraries(TheSeedTests PRIVATE
    glfw
    imgui::imgui
    GLEW::GLEW
    OpenGL::GL
    SQLite::SQLite3
    magic_enum::magic_enum
    Threads::Threads
)

if(nlohmann_json_FOUND)
    target_link_libraries(TheSeedTests PRIVATE nlohmann_json::nlohmann_json)
endif()

if(SODIUM_FOUND)
    target_link_libraries(TheSeedTests PRIVATE ${SODIUM_LIBRARIES})
endif()

if(OpenSSL_FOUND)
    target_link_libraries(TheSeedTests PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(WIN32)
    target_link_libraries(TheSeedTests PRIVATE ws2_32 iphlpapi)
endif()

# =============================================================================
# CTest Integration
# =============================================================================
enable_testing()
add_test(NAME TheSeedUnitTests COMMAND TheSeedTests)
