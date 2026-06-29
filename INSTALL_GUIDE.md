# THE SEED V13.2 — Installations-Guide

## Datei-Zuordnung (Repository-Struktur)

```
TheSeed/
├── vcpkg.json                          → ROOT (ersetzt bestehende)
├── CMakeLists.txt                      → ROOT (ersetzt bestehende)
├── main.cpp                            → ROOT (ersetzt bestehende)
│
├── core/
│   ├── ECS.h                           → core/ (ersetzt bestehende)
│   └── (GameSystems.cpp, Database.cpp etc. bleiben unverändert)
│
├── ecs/
│   ├── Components.h                    → ecs/ (NEU)
│   ├── ecs_Archetype.h                 → ecs/ (bereits vorhanden)
│   ├── ecs_Chunk.h                     → ecs/ (bereits vorhanden)
│   ├── ecs_ComponentTraits.h           → ecs/ (bereits vorhanden)
│   ├── ecs_ECS.h                       → ecs/ (bereits vorhanden)
│   ├── ecs_EcsWorld.h                  → ecs/ (bereits vorhanden)
│   ├── ecs_EntityManager.h             → ecs/ (bereits vorhanden)
│   ├── ecs_Example.cpp                 → ecs/ (bereits vorhanden)
│   ├── ecs_Query.h                     → ecs/ (bereits vorhanden)
│   └── ecs_Types.h                     → ecs/ (bereits vorhanden)
│
├── network/                            → network/ (NEUER ORDNER)
│   ├── network_UdpSocket.h             → network/
│   ├── network_UdpSocket.cpp           → network/
│   ├── network_ReliableUdp.h           → network/
│   ├── network_ReliableUdp.cpp       → network/
│   ├── network_NetworkServer.h       → network/
│   └── network_NetworkServer.cpp     → network/
│
├── server/
│   ├── Server.h                        → server/ (ersetzt bestehende)
│   ├── Server.cpp                      → server/ (ersetzt bestehende)
│   ├── Network.h                       → server/ (bleibt vorhanden, wird deprecated)
│   ├── Network.cpp                     → server/ (bleibt vorhanden, wird deprecated)
│   ├── PacketHandler.h                 → server/ (bleibt vorhanden)
│   ├── PacketHandler.cpp               → server/ (bleibt vorhanden)
│   ├── Validation.h                    → server/ (bleibt vorhanden)
│   ├── Validation.cpp                  → server/ (bleibt vorhanden)
│   └── auth/                           → server/auth/ (NEUER ORDNER)
│       ├── AuthService.h               → server/auth/
│       └── AuthService.cpp             → server/auth/
│
├── client/
│   ├── ClientTick.h                    → client/ (bleibt vorhanden)
│   ├── ClientTick.cpp                  → client/ (bleibt vorhanden)
│   ├── Connection.h                    → client/ (bleibt vorhanden)
│   ├── Connection.cpp                  → client/ (bleibt vorhanden)
│   ├── Renderer.h                      → client/ (bleibt vorhanden)
│   └── Renderer.cpp                    → client/ (bleibt vorhanden)
│
├── editor/
│   ├── EditorRuntime.h                 → editor/ (bleibt vorhanden)
│   ├── EditorRuntime.cpp               → editor/ (bleibt vorhanden)
│   ├── EditorPanels.h                  → editor/ (bleibt vorhanden)
│   ├── EditorPanels.cpp                → editor/ (bleibt vorhanden)
│   ├── EntityEditor.h                  → editor/ (bleibt vorhanden)
│   ├── EntityEditor.cpp                → editor/ (bleibt vorhanden)
│   ├── CommandSystem.h                 → editor/ (bleibt vorhanden)
│   ├── CommandSystem.cpp               → editor/ (bleibt vorhanden)
│   ├── AssetDatabase.h                 → editor/ (bleibt vorhanden)
│   ├── AssetDatabase.cpp               → editor/ (bleibt vorhanden)
│   ├── DataDefinitionEditor.h          → editor/ (bleibt vorhanden)
│   └── DataDefinitionEditor.cpp        → editor/ (bleibt vorhanden)
│
├── memory/
│   ├── PoolAllocator.h                 → memory/ (bereits vorhanden)
│   ├── PoolAllocator.cpp               → memory/ (bereits vorhanden)
│   ├── StackAllocator.h                → memory/ (bereits vorhanden)
│   ├── StackAllocator.cpp              → memory/ (bereits vorhanden)
│   ├── FreelistAllocator.h             → memory/ (bereits vorhanden)
│   ├── FreelistAllocator.cpp           → memory/ (bereits vorhanden)
│   ├── SlabAllocator.h                 → memory/ (bereits vorhanden)
│   └── SlabAllocator.cpp               → memory/ (bereits vorhanden)
│
├── math/
│   ├── MathCommon.h                    → math/ (bereits vorhanden)
│   ├── Vector.h                        → math/ (bereits vorhanden)
│   ├── Matrix.h                        → math/ (bereits vorhanden)
│   ├── Quaternion.h                    → math/ (bereits vorhanden)
│   └── Frustum.h                       → math/ (bereits vorhanden)
│
├── renderer_vulkan/
│   ├── VulkanContext.h                 → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanContext.cpp               → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanSwapchain.h               → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanSwapchain.cpp             → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanQueue.h                   → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanQueue.cpp                 → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanDebug.h                   → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanDebug.cpp                 → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanRenderPass.h              → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanRenderPass.cpp            → renderer_vulkan/ (bereits vorhanden)
│   ├── VulkanResourceManager.h         → renderer_vulkan/ (bereits vorhanden)
│   └── VulkanResourceManager.cpp       → renderer_vulkan/ (bereits vorhanden)
│
├── build.bat                           → ROOT (bereits vorhanden)
├── build.sh                            → ROOT (bereits vorhanden)
└── TheSeed_AAA_Roadmap.pdf             → ROOT (bereits vorhanden)
```

## Installationsschritte

### 1. Ordner erstellen
```bash
# Im Repository-Root
mkdir -p network
mkdir -p server/auth
```

### 2. Dateien kopieren
```bash
# Root-Dateien
cp vcpkg.json ./
cp CMakeLists.txt ./
cp main.cpp ./

# Core
cp ECS.h core/

# ECS
cp Components.h ecs/

# Network (NEU)
cp network_*.h network/
cp network_*.cpp network/

# Server
cp Server.h server/
cp Server.cpp server/
cp AuthService.h server/auth/
cp AuthService.cpp server/auth/
```

### 3. vcpkg Dependencies installieren
```bash
vcpkg install --triplet x64-windows  # oder x64-linux
```

### 4. Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
 cmake --build . --config Release
```

## Was wird ersetzt vs. was bleibt

| Datei | Aktion | Grund |
|-------|--------|-------|
| `vcpkg.json` | **Ersetzen** | Neue Dependencies (vk-bootstrap, libsodium) |
| `CMakeLists.txt` | **Ersetzen** | Network-Sources hinzugefügt, vk-bootstrap Link |
| `main.cpp` | **Ersetzen** | ECS-World Init, neue Komponenten |
| `core/ECS.h` | **Ersetzen** | Legacy-Wrapper + gEcsWorld Export |
| `server/Server.h` | **Ersetzen** | GameServer Klasse hinzugefügt |
| `server/Server.cpp` | **Ersetzen** | UDP Server Integration |
| `ecs/Components.h` | **Neu** | Alle Game-Components für ECS |
| `network/*` | **Neu** | UDP + Reliable UDP Layer |
| `server/auth/*` | **Neu** | Argon2id + JWT Auth |
| `core/GameSystems.cpp` | **Bleibt** | Unverändert (wird später migriert) |
| `editor/*` | **Bleibt** | Unverändert |
| `renderer_vulkan/*` | **Bleibt** | Noch nicht aktiv (AP-01 folgt) |

## Bekannte TODOs nach Installation

1. `server/PacketHandler.cpp` muss auf ECS-Queries umgestellt werden
2. `client/ClientTick.cpp` muss UDP statt TCP nutzen
3. `core/GameSystems.cpp` muss ECS-Components verwenden
4. `editor/EditorRuntime.cpp` muss ECS-Entity-Outliner nutzen
