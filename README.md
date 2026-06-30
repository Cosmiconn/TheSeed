# TheSeed Engine V13.2

C++23 MMORPG Engine mit Archetype-ECS, Vulkan-Renderer und Multi-Threaded Server.

## Features

- **ECS**: Archetype-basiertes Entity-Component-System mit SOA-Speicher
- **Netzwerk**: UDP-basiert mit Reliable Channel (SACK, RTT-Schätzung)
- **Renderer**: OpenGL (Legacy) + Vulkan (optional)
- **Editor**: ImGui-basierter In-Engine Editor
- **Multi-Threading**: Lock-Free Work-Stealing Thread Pool
- **Persistenz**: SQLite mit async Writer-Thread

## Build

### Voraussetzungen
- CMake 3.25+
- C++23 Compiler (GCC 13+, Clang 16+, MSVC 2022+)
- vcpkg

### Windows
```batch
git clone https://github.com/Cosmiconn/TheSeed.git
cd TheSeed
vcpkg install
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### Linux
```bash
git clone https://github.com/Cosmiconn/TheSeed.git
cd TheSeed
vcpkg install
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Architektur

```
TheSeed/
├── core/         # Kern-Engine (World, GameSystems, Database, Log)
├── ecs/          # Archetype-ECS (EcsWorld, Components, Query)
├── network/      # UDP + Reliable UDP
├── server/       # Multi-Threaded Game Server
├── client/       # Client-Logik + Renderer
├── editor/       # ImGui Editor
├── renderer_vulkan/  # Vulkan Renderer (optional)
├── math/         # Mathematik-Bibliothek
└── memory/       # Speicher-Allokatoren
```

## ECS-Systeme

| System | Beschreibung |
|--------|-------------|
| Movement | Aktualisiert Position basierend auf Velocity |
| Health | Prüft auf Tod, verwaltet HP |
| Combat | Verarbeitet eingehenden Schaden |
| StatusEffects | Verarbeitet Buffs/Debuffs |
| AI | Einfaches Aggro-Verhalten für Monster |

## Netzwerk-Protokoll

- **Protokoll-ID**: 0x4D4D ("MM")
- **Pakettypen**: MOVE, COMBAT, CHAT, INTERACT, SNAPSHOT
- **Snapshot-Rate**: 20Hz
- **Reliable**: SACK-basiert mit RTT-Schätzung

## Changelog

Siehe [Changelog_0001.md](Changelog_0001.md) und [Changelog_0002.md](Changelog_0002.md)

## Lizenz

Proprietär — Alle Rechte vorbehalten.
