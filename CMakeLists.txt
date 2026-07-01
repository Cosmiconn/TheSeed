cmake_minimum_required(VERSION 3.25)
# =============================================================================
# TheSeed Engine — CMake Build Configuration (V13.2 P3-FIX)
# =============================================================================
# KORREKTUR P3:
# • libsodium, nlohmann-json, OpenSSL als Abhängigkeiten hinzugefügt
# • PostgreSQL und Redis als optionale Features
# • Korrekte Link-Reihenfolge für Auth-Module
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
    add_compile_options(/MP)
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
# Abhängigkeiten finden (Basis)
# =============================================================================
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Threads REQUIRED)

# =============================================================================
# Auth-Abhängigkeiten (P3-FIX)
# =============================================================================
# nlohmann-json (für JWT Payload)
find_package(nlohmann_json CONFIG)
if(nlohmann_json_FOUND)
    add_compile_definitions(HAS_NLOHMANN_JSON)
endif()

# libsodium (für Argon2id)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM libsodium)
    if(SODIUM_FOUND)
        add_compile_definitions(HAS_LIBSODIUM)
        message(STATUS "libsodium gefunden: Argon2id aktiviert")
    else()
        message(WARNING "libsodium nicht gefunden. Fallback-Hashing wird verwendet.")
    endif()
endif()

# OpenSSL (für HMAC-SHA256 JWT Signing)
find_package(OpenSSL)
if(OpenSSL_FOUND)
    add_compile_definitions(HAS_OPENSSL)
    message(STATUS "OpenSSL gefunden: JWT HMAC-SHA256 aktiviert")
else()
    message(WARNING "OpenSSL nicht gefunden. JWT-Signing deaktiviert.")
endif()

# =============================================================================
# Vulkan (optional)
# =============================================================================
find_package(Vulkan)
if(Vulkan_FOUND)
    add_compile_definitions(VULKAN_AVAILABLE)
endif()

# =============================================================================
# PostgreSQL (optional, für Auth Service)
# =============================================================================
find_package(PostgreSQL)
if(PostgreSQL_FOUND)
    add_compile_definitions(POSTGRESQL_AVAILABLE)
    message(STATUS "PostgreSQL gefunden: PostgreSqlUserRepository aktiviert")
else()
    message(WARNING "PostgreSQL nicht gefunden. PostgreSqlUserRepository wird gestubbt.")
endif()

# =============================================================================
# Redis/hiredis (optional, für Rate Limiting)
# =============================================================================
if(PkgConfig_FOUND)
    pkg_check_modules(HIREDIS hiredis)
    if(HIREDIS_FOUND)
        add_compile_definitions(REDIS_AVAILABLE)
        message(STATUS "hiredis gefunden: RedisRateLimiter aktiviert")
    else()
        message(WARNING "hiredis nicht gefunden. RedisRateLimiter wird gestubbt.")
    endif()
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

set(SERVER_AI_SOURCES
    server/ai/AIBehavior.cpp
    server/ai/AIParallelSystem.cpp
    server/ai/Pathfinding.cpp
    server/ai/NavMesh.cpp
)

set(SERVER_AUTH_SOURCES
    server/auth/AuthService.cpp
    server/auth/SqliteUserRepository.cpp
)

# FIX P3: Optionale Auth-Backends
if(PostgreSQL_FOUND)
    list(APPEND SERVER_AUTH_SOURCES server/auth/PostgreSqlUserRepository.cpp)
endif()

if(HIREDIS_FOUND)
    list(APPEND SERVER_AUTH_SOURCES server/auth/RedisRateLimiter.cpp)
endif()

set(SERVER_COMBAT_SOURCES
    server/combat/CombatSystem.cpp
)

set(SERVER_NETWORK_SOURCES
    server/network/Snapshot.cpp
)

set(CLIENT_SOURCES
    client/Connection.cpp
    client/ClientTick.cpp
    client/Renderer.cpp
    client/Interpolation.cpp
    client/Animation.cpp
)

set(EDITOR_SOURCES
    editor/EditorRuntime.cpp
    editor/EditorPanels.cpp
    editor/AssetDatabase.cpp
    editor/CommandSystem.cpp
    editor/DataDefinitionEditor.cpp
    editor/EntityEditor.cpp
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
    ${SERVER_AI_SOURCES}
    ${SERVER_AUTH_SOURCES}
    ${SERVER_COMBAT_SOURCES}
    ${SERVER_NETWORK_SOURCES}
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

# FIX P3: libsodium Include-Verzeichnis
if(SODIUM_FOUND)
    target_include_directories(TheSeed PRIVATE ${SODIUM_INCLUDE_DIRS})
endif()

# FIX P3: hiredis Include-Verzeichnis
if(HIREDIS_FOUND)
    target_include_directories(TheSeed PRIVATE ${HIREDIS_INCLUDE_DIRS})
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

# FIX P3: Auth-Abhängigkeiten verlinken
if(nlohmann_json_FOUND)
    target_link_libraries(TheSeed PRIVATE nlohmann_json::nlohmann_json)
endif()

if(SODIUM_FOUND)
    target_link_libraries(TheSeed PRIVATE ${SODIUM_LIBRARIES})
endif()

if(OpenSSL_FOUND)
    target_link_libraries(TheSeed PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(PostgreSQL_FOUND)
    target_link_libraries(TheSeed PRIVATE PostgreSQL::PostgreSQL)
endif()

if(HIREDIS_FOUND)
    target_link_libraries(TheSeed PRIVATE ${HIREDIS_LIBRARIES})
endif()

if(Vulkan_FOUND)
    target_link_libraries(TheSeed PRIVATE Vulkan::Vulkan)
endif()

# =============================================================================
# Windows-spezifische Bibliotheken
# =============================================================================
if(WIN32)
    target_link_libraries(TheSeed PRIVATE
        ws2_32
        iphlpapi
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
