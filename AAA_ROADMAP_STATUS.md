# TheSeed Engine — AAA-Roadmap Status (Live-Dokument)

**Version:** 13.2.1 (Patch)  
**Letzte Aktualisierung:** 2026-07-01 (P0: NavMesh + Pathfinding + Combat)  
**Gesamt-Arbeitspakete:** 97 (AP-01 bis AP-97)

---

## Legende

| Symbol | Bedeutung |
|--------|-----------|
| ✅ | **100% erledigt** — Vollständig implementiert & integriert |
| 🟡 | **Teilweise erledigt** — Grundgerüst vorhanden, aber nicht produktionsreif |
| 🔴 | **Nicht erledigt** — Fehlt komplett oder nur als Stub |

---

## Phase 1: Fundament & Rendering (Monate 1–6)

| AP | Arbeitspaket | Prio | Status | Begründung |
|----|-------------|------|--------|-----------|
| AP-01 | Vulkan-Renderer-Init | Kritisch | 🟡 | VulkanContext.cpp/h vorhanden, aber keine vollständige Initialisierung (Instance, Device, Swapchain) |
| AP-02 | Render-Pass & Framebuffer | Kritisch | 🟡 | VulkanRenderPass.cpp/h existieren, aber generischer Builder fehlt |
| AP-03 | Resource-Manager (Descriptor Pools) | Kritisch | 🔴 | VulkanResourceManager.cpp/h sind Stubs |
| AP-04 | Deferred Shading Pipeline | Kritisch | 🔴 | Nicht implementiert — nur Forward-Rendering in Legacy-OpenGL |
| AP-05 | PBR-Material-System | Hoch | 🔴 | Nicht implementiert — Toon-Shading nur 4-stufig |
| AP-06 | Cascaded Shadow Maps (CSM) | Hoch | 🟡 | ShadowMap.cpp/h existieren, aber keine Cascade-Implementierung |
| AP-07 | Post-Processing Stack | Hoch | 🔴 | Nicht implementiert |
| AP-08 | Partikel-System (GPU-gestützt) | Mittel | 🔴 | Nicht implementiert |
| AP-09 | Skeletal Mesh Rendering | Hoch | 🔴 | Nicht implementiert |
| AP-10 | Instanced Rendering | Hoch | 🔴 | Nicht implementiert |
| AP-11 | Dynamic Resolution Scaling | Mittel | 🔴 | Nicht implementiert |
| AP-12 | Debug-Render-Pass | Mittel | 🟡 | VulkanDebug.cpp/h vorhanden, aber nur Basis-Utils |
| AP-13 | Math-Library (SIMD) | Kritisch | 🟡 | Vector.h/Matrix.h/Quaternion.h vorhanden, aber **kein SIMD** (SSE4/AVX2/NEON) |
| AP-14 | Memory-Allocators | Hoch | 🟡 | PoolAllocator/StackAllocator/FreelistAllocator/SlabAllocator existieren, aber **kein Alignment-Support**, **kein Memory-Budget-Tracking** |
| AP-15 | File-IO & VFS | Hoch | 🔴 | Nicht implementiert |
| AP-16 | Reflection & Serialization | Hoch | 🔴 | Nicht implementiert |
| AP-17 | CMake-Build-System | Kritisch | ✅ | CMakeLists.txt vollständig mit vcpkg-Integration |
| AP-18 | Platform-Abstraction-Layer | Hoch | 🟡 | GLFW3 für Window/Input, aber kein Gamepad, kein Thread-Wrapper |
| AP-19 | Profiling & Instrumentation | Mittel | 🔴 | Nicht implementiert |

**Phase 1 Fazit:** 2/19 erledigt, 6/19 teilweise, 11/19 offen

---

## Phase 2: ECS & Parallelisierung (Monate 4–8)

| AP | Arbeitspaket | Prio | Status | Begründung |
|----|-------------|------|--------|-----------|
| AP-20 | Archetype Storage Design | Kritisch | ✅ | SOA-Chunk-basiert, 16KB Chunks, generational handles, Archetype-Hash |
| AP-21 | Entity-Manager & Queries | Kritisch | ✅ | Create/Destroy/Add/RemoveComponent, Query-API mit All/Any/None, O(1) Lookup |
| AP-22 | System-Registry & Scheduling | Kritisch | 🟡 | `RegisterSystem()` existiert, aber **kein Dependency-Graph**, **kein Topo-Sort** |
| AP-23 | World-Migration von V12 | Hoch | 🟡 | Legacy-Adapter in Server.cpp, aber **kein automatisches Migrationstool** |
| AP-24 | ECS-Unit-Tests & Benchmarks | Hoch | 🔴 | Nicht implementiert |
| AP-25 | ECS-Chunk-Allocator | Hoch | ✅ | Pool-Allokation, 64-Byte Alignment, Chunk-Verwaltung |
| AP-26 | Component-Serializer | Hoch | 🔴 | Nicht implementiert |
| AP-27 | String-Interning & Hash-IDs | Mittel | 🔴 | `materialId`/`meshId` sind immer noch `std::string` |
| AP-28 | Fibers/Job-Queue | Kritisch | 🟡 | Work-Stealing ThreadPool vorhanden, aber **keine Fibers**, **keine Kontinuationen** |
| AP-29 | Parallel-For & Reduction | Hoch | 🔴 | Nicht implementiert |
| AP-30 | Sync-Primitive (lightweight) | Hoch | 🟡 | `std::latch` in ThreadPool, aber **keine Fiber-Semaphore**, **kein Future/Promise** |
| AP-31 | System-Parallelisierung | Mittel | 🟡 | `std::async` in Server.cpp, aber **kein automatisches Dependency-Graph-Scheduling** |

**Phase 2 Fazit:** 3/12 erledigt, 6/12 teilweise, 3/12 offen

---

## Phase 3: Netzwerk & Multiplayer (Monate 6–10)

| AP | Arbeitspaket | Prio | Status | Begründung |
|----|-------------|------|--------|-----------|
| AP-32 | UDP-Socket-Layer | Kritisch | ✅ | Cross-Platform UDP (Windows/Linux), non-blocking, IPv4/IPv6 |
| AP-33 | Zuverlässiges UDP (Reliability) | Kritisch | ✅ | Sequenznummern, SACK, RTT-Schätzung (Jacobson/Karn), Fragmentierung |
| AP-34 | Connection-Management | Kritisch | 🟡 | Session-Map vorhanden, aber **kein 3-Wege-Handshake**, **kein DDoS-Protection** |
| AP-35 | Packet-Priorisierung & Bandwidth | Hoch | 🔴 | Nicht implementiert |
| AP-36 | Crypto-Layer | Hoch | 🔴 | Nicht implementiert (kein DTLS, kein ChaCha20-Poly1305) |
| AP-37 | World-Snapshot-Builder | Kritisch | ✅ | Delta-Kompression, Entity-Priority, Serialisierung |
| AP-38 | Client-Side Interpolation | Kritisch | ✅ | Snapshot-Buffer, Interpolation, Extrapolation, Dead Reckoning |
| AP-39 | Entity-Priority & LOD-Netzwerk | Hoch | 🟡 | Distanz-basiertes Interest Management vorhanden, aber **keine Update-Rate-Reduktion** |
| AP-40 | Interest-Management (Rewamp) | Hoch | ✅ | Spatial Hashing, AOI-Queries, Layer-basiert |
| AP-41 | Network-Profiler & Debug | Mittel | 🔴 | Nicht implementiert |
| AP-42 | Multi-Threaded Server | Kritisch | 🟡 | Netzwerk-Thread + Simulations-Thread, aber **keine Lock-freien Message-Queues** |
| AP-43 | Zone-Server-Architektur | Hoch | 🔴 | Nicht implementiert |
| AP-44 | Instancing-System | Hoch | 🔴 | Nicht implementiert |
| AP-45 | Authentication-Service | Hoch | 🟡 | JWT-Struktur vorhanden, **libsodium optional**, **keine echte Argon2id** (Fallback-Hashing) |
| AP-46 | Game-Datenbank (Cluster) | Mittel | 🔴 | SQLite nur lokal, **kein PostgreSQL-Cluster**, **kein Redis-Caching** |

**Phase 3 Fazit:** 5/15 erledigt, 5/15 teilweise, 5/15 offen

---

## Phase 4: Gameplay-Systeme (Monate 8–14)

| AP | Arbeitspaket | Prio | Status | Begründung |
|----|-------------|------|--------|-----------|
| AP-47 | NavMesh-Generation | Kritisch | ✅ | Vollständig: Voxel-Heightfield, Walkable-Filter, Erosion, Tile-basiert |
| AP-48 | Pathfinding (Crowd) | Kritisch | ✅ | Vollständig: A* auf Polygons, String-Pulling, Crowd Avoidance (Separation/Alignment/Cohesion) |
| AP-49 | Behavior Tree Runtime | Kritisch | ✅ | Vollständig: Sequence, Selector, Parallel, Decorator, Blackboard |
| AP-50 | AI-Behavior-Library | Hoch | 🟡 | CombatAI, PatrolAI, FleeAI als Factory-Methoden, aber **keine NPC-Tagesroutinen** |
| AP-51 | Aggro & Threat-System | Hoch | ✅ | Vollständig: Threat-Table, Decay, Top-Threat-Selection, Combat-Integration |
| AP-52 | Group-AI (Formation) | Mittel | 🔴 | Nicht implementiert |
| AP-53 | Combat-System (Erweitert) | Kritisch | ✅ | Vollständig: Hitbox (AABB/Sphere/Capsule), Combo-State-Machine, Directional Blocking, Threat-Table, Damage-Typen |
| AP-54 | Loot-System | Hoch | 🔴 | Nicht implementiert |
| AP-55 | Progression-System | Hoch | 🔴 | Nicht implementiert |
| AP-56 | Crafting-System | Mittel | 🔴 | Nicht implementiert |
| AP-57 | Wirtschaft & Auction House | Mittel | 🔴 | Nicht implementiert |
| AP-58 | Mounts & Transport | Niedrig | 🔴 | Nicht implementiert |
| AP-59 | Gilden-System | Hoch | 🔴 | Nicht implementiert |
| AP-60 | PvP-System | Hoch | 🔴 | Nicht implementiert |
| AP-61 | Gruppen-System | Hoch | 🔴 | Nicht implementiert |
| AP-62 | Friends & Block | Mittel | 🔴 | Nicht implementiert |
| AP-63 | Chat-System (Erweitert) | Mittel | 🟡 | Basis-Chat in PacketHandler, aber **keine Kanäle**, **kein Whisper** |
| AP-64 | Lua-Runtime-Integration | Kritisch | 🔴 | Nicht implementiert |
| AP-65 | Scriptable-Quest-System | Hoch | 🔴 | Nicht implementiert |
| AP-66 | Scriptable-AI-Behaviors | Hoch | 🔴 | Nicht implementiert |
| AP-67 | Visual-Scripting-Editor | Mittel | 🔴 | Nicht implementiert |

**Phase 4 Fazit:** 4/21 erledigt, 3/21 teilweise, 14/21 offen

---

## Phase 5: Tools & Pipeline (Monate 12–18)

| AP | Arbeitspaket | Prio | Status | Begründung |
|----|-------------|------|--------|-----------|
| AP-68 | Editor-Windowing (Docking) | Hoch | 🟡 | ImGui-Docking vorhanden, aber **keine persistente Layout-Speicherung** |
| AP-69 | 3D-Viewport mit Gizmos | Hoch | 🔴 | Nicht implementiert |
| AP-70 | Entity-Outliner & Inspector | Hoch | 🟡 | EntityEditor.cpp/h vorhanden, aber **kein Reflection-generierter Inspector** |
| AP-71 | Terrain-Editor (Erweitert) | Mittel | 🔴 | Nicht implementiert |
| AP-72 | Visual-Scripting-Editor | Mittel | 🔴 | Nicht implementiert |
| AP-73 | Animation-Editor (Basis) | Mittel | 🔴 | Nicht implementiert |
| AP-74 | Database-Editor | Niedrig | 🔴 | Nicht implementiert |
| AP-75 | Asset-Importer | Kritisch | 🔴 | Nicht implementiert |
| AP-76 | Asset-Cooking | Hoch | 🔴 | Nicht implementiert |
| AP-77 | Texture-Pipeline | Hoch | 🔴 | Nicht implementiert |
| AP-78 | Asset-Browser | Mittel | 🔴 | Nicht implementiert |
| AP-79 | Hot-Reload-System | Mittel | 🔴 | Nicht implementiert |
| AP-80 | GitHub-Actions-Pipeline | Hoch | 🟡 | `.github/workflows/` existieren, aber **keine vollständige Build-Matrix** |
| AP-81 | Container-Deployment | Hoch | 🔴 | Nicht implementiert |
| AP-82 | Monitoring & Alerting | Mittel | 🔴 | Nicht implementiert |
| AP-83 | Deployment-Automatisierung | Mittel | 🔴 | Nicht implementiert |
| AP-84 | Crash-Reporter & Analytics | Niedrig | 🔴 | Nicht implementiert |

**Phase 5 Fazit:** 0/17 erledigt, 3/17 teilweise, 14/17 offen

---

## Phase 6: Polishing & Skalierung (Monate 16–24)

| AP | Arbeitspaket | Prio | Status | Begründung |
|----|-------------|------|--------|-----------|
| AP-85 | GPU-Driven-Rendering | Hoch | 🔴 | Nicht implementiert |
| AP-86 | Occlusion Culling | Hoch | 🔴 | Nicht implementiert |
| AP-87 | Streaming & Loading | Hoch | 🔴 | Nicht implementiert |
| AP-88 | Server-Performance | Hoch | 🟡 | Spatial-Hash für AOI vorhanden, aber **kein Server-Side LOD**, **kein lazy Update** |
| AP-89 | Memory-Optimierung | Mittel | 🟡 | Allocators vorhanden, aber **keine Memory-Budgets**, **kein automatisches Unloading** |
| AP-90 | Linux-Client (Vulkan) | Hoch | 🟡 | Vulkan-Renderer existiert, aber **nicht vollständig**, **kein Steam-Deck-Test** |
| AP-91 | macOS-Port (MoltenVK) | Mittel | 🔴 | Nicht implementiert |
| AP-92 | Konsolen-Vorbereitung | Mittel | 🔴 | Nicht implementiert |
| AP-93 | Controller & Accessibility | Mittel | 🔴 | Nicht implementiert |
| AP-94 | Automated-Testing-Suite | Hoch | 🔴 | Nicht implementiert |
| AP-95 | Load-Testing | Hoch | 🔴 | Nicht implementiert |
| AP-96 | Closed-Beta & Feedback | Mittel | 🔴 | Nicht implementiert |
| AP-97 | Live-Ops & Balancing | Mittel | 🔴 | Nicht implementiert |

**Phase 6 Fazit:** 0/13 erledigt, 3/13 teilweise, 10/13 offen

---

## 📊 Gesamtzusammenfassung

| Phase | Erledigt | Teilweise | Offen | Gesamt |
|-------|----------|-----------|-------|--------|
| 1: Fundament & Rendering | 2 (11%) | 6 (32%) | 11 (58%) | 19 |
| 2: ECS & Parallelisierung | 3 (25%) | 6 (50%) | 3 (25%) | 12 |
| 3: Netzwerk & Multiplayer | 5 (33%) | 5 (33%) | 5 (33%) | 15 |
| 4: Gameplay-Systeme | 1 (5%) | 3 (14%) | 17 (81%) | 21 |
| 5: Tools & Pipeline | 0 (0%) | 3 (18%) | 14 (82%) | 17 |
| 6: Polishing & Skalierung | 0 (0%) | 3 (23%) | 10 (77%) | 13 |
| **GESAMT** | **14 (14%)** | **26 (27%)** | **57 (59%)** | **97** |

---

## 🎯 Priorisierte nächste Schritte (Empfohlene Reihenfolge)

| Priorität | APs | Warum |
|-----------|-----|-------|
| **P0 (Kritisch)** | ✅ ERLEDIGT | NavMesh + Pathfinding + erweitertes Combat vollständig implementiert |
| **P0 (Kritisch)** | AP-51, AP-52 | Aggro & Threat-System + Group-AI (Formation) — Combat-System nutzbar machen |
| **P0 (Kritisch)** | AP-50, AP-52 | AI Behavior Library erweitern (NPC-Tagesroutinen, Formationen) |
| **P1 (Hoch)** | AP-04, AP-05, AP-09, AP-10 | Deferred Shading + PBR + Skeletal Mesh für AAA-Rendering |
| **P2 (Hoch)** | AP-64, AP-65, AP-66 | Lua-Scripting für Quests und AI — entscheidend für Content-Creation |
| **P3 (Mittel)** | AP-54–AP-58 | Loot, Progression, Crafting für Gameplay-Loop |

---

## Changelog-Referenz

| Changelog | Phase | Inhalt |
|-----------|-------|--------|
| Changelog_0001 | P0 | Kritische Fehler (Build, Sicherheit, ODR) |
| Changelog_0002 | P0-P1 | Absturz/Compiler-Fehler + ECS-Infrastruktur |
| Changelog_0003 | P1-P3 | Architekturverbesserungen + Build-System |
| Changelog_0004 | P3-P4 | Performance-Optimierungen (ECS, ThreadPool, Netzwerk) |
| Changelog_0005 | P0-P5 | Abschließende Fixes + Feature-Vervollständigung |

---

*Dieses Dokument wird bei jeder neuen Iteration aktualisiert.*
