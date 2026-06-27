Ich werde die Roadmap nach Priorität durchgehen, beginnend mit den Kritischen Arbeitspaketen, dann Hoch, Mittel, Niedrig. Das ergibt einen strukturierten, umsetzungsorientierten Plan.
PRIORITÄT: KRITISCH (Must-Have für AAA-Fundament)
AP-01: Vulkan-Renderer-Init
Phase: 1 | Modul: renderer_vulkan/ | Abhängigkeiten: Keine
Ziel: Vulkan-Instance, Device, Swapchain, Queue-Familien; valide Vortestung auf Win/Linux; Validation-Layers fehlerfrei.
Status V13.1: OpenGL 3.0 in client/Renderer.cpp — kompletter Ersatz nötig.
Empfohlene Bibliothek: vk-bootstrap (MIT) — reduziert Boilerplate von ~2000 LOC auf ~200.
Struktur:
renderer_vulkan/
├── VulkanContext.h/cpp      # Instance, Device, PhysicalDevice
├── VulkanSwapchain.h/cpp    # Swapchain, Surface, Present-Queue
├── VulkanQueue.h            # Graphics/Compute/Transfer Queues
└── VulkanDebug.h/cpp        # Validation Layers, Debug Messenger
Akzeptanzkriterien:
•  [ ] Triangle auf Windows 10/11
•  [ ] Triangle auf Linux (Wayland + X11)
•  [ ] Validation Layers 0 Warnungen/Fehler
•  [ ] vk-bootstrap integriert
AP-17: CMake-Build-System + vcpkg
Phase: 1 | Modul: Root | Abhängigkeiten: Keine
Ziel: Professionalisierung des bestehenden CMakeLists.txt mit vcpkg-Manifest.
Status V13.1: CMake existiert, aber Dependencies sind kommentiert/hardcoded.
Änderungen an CMakeLists.txt:
NEU: vcpkg Manifest
vcpkg.json im Root:
{
"name": "the-seed-engine",
"version": "13.1",
"dependencies": [
"glfw3",
"sqlite3",
"imgui",
"glm",
"vulkan",
"vk-bootstrap"
]
}
Akzeptanzkriterien:
•  [ ] cmake --preset=default konfiguriert alles automatisch
•  [ ] Debug/Release/RelWithDebInfo-Profile
•  [ ] CI-Build auf GitHub Actions (Windows + Linux)
AP-20: Archetype Storage Design
Phase: 2 | Modul: core/ecs/ | Abhängigkeiten: AP-14 (Memory-Allocators, empfohlen vorher)
Ziel: SOA-Chunk-basierte Speicherung (16KB Chunks), generational handles, Archetype-Hash aus Component-Bitset.
Status V13.1: AOS-Structs in core/ECS.h, std::vector<Entity> serverRegistry.
Neue Struktur:
// core/ecs/Archetype.h
struct Archetype {
std::vector<ComponentType> componentTypes;
std::vector<Chunk*> chunks;  // 16KB each
size_t hash;  // Bitset-Hash
};
struct Chunk {
static constexpr size_t SIZE = 16384;  // 16KB
alignas(64) std::byte storage[SIZE];
uint32_t entityCount = 0;
uint32_t capacity = 0;  // entities that fit
};
Akzeptanzkriterien:
•  [ ] 10.000 Entities allozieren in <10ms
•  [ ] Cache-Miss-Rate <5% bei sequentiellem Zugriff
•  [ ] Memory-Overhead <20% gegenüber raw arrays
AP-21: Entity-Manager & Queries
Phase: 2 | Modul: core/ecs/ | Abhängigkeiten: AP-20
Ziel: Create/Destroy/Add/RemoveComponent, Query-API mit All/Any/None-Filtern, Query-Caching, O(1) Lookup.
Status V13.1: nextEntityId++, std::ranges::find_if (O(n)).
Neue API:
// core/ecs/World.h
class EcsWorld {
public:
EntityHandle CreateEntity();
void DestroyEntity(EntityHandle e);
template<typename... Cs>
void AddComponents(EntityHandle e, Cs&&... comps);

template<typename... Cs>
void RemoveComponents(EntityHandle e);

// Query: All<Transform, Health>, Any<Buff>, None<DeadTag>
template<typename... Filters>
QueryResult Query();

};
Akzeptanzkriterien:
•  [ ] Create/Destroy in O(1) amortisiert
•  [ ] Query 10k Entities in <1ms
•  [ ] Query-Caching: wiederholte Queries O(1)
AP-22: System-Registry & Scheduling
Phase: 2 | Modul: core/ecs/ | Abhängigkeiten: AP-21
Ziel: System-Registration mit Phase/Group-Tag, automatisches Dependency-Graph-Building, Topo-Sort.
Status V13.1: Hardcoded 7-Phasen-Tick in server/Server.cpp.
Neue API:
// core/ecs/System.h
struct SystemDescriptor {
std::string name;
Phase phase;  // PreUpdate, Update, PostUpdate, Render
std::vector<ComponentAccess> reads;
std::vector<ComponentAccess> writes;
std::function<void(EcsWorld&)> execute;
};
class SystemRegistry {
public:
void Register(SystemDescriptor desc);
void ExecutePhase(Phase p);  // Topo-sortiert, parallel wo möglich
};
Akzeptanzkriterien:
•  [ ] Dependency-Cycle-Detection
•  [ ] Automatische Parallelisierung bei disjunkten Writes
•  [ ] V13.1-Systeme (Movement, Combat, Respawn) als ECS-Systeme migriert
AP-32: UDP-Socket-Layer
Phase: 3 | Modul: server/network/ | Abhängigkeiten: Keine (ersetzt AP-34 teilweise)
Ziel: Non-blocking UDP-Sockets, IPv4/IPv6-Dual-Stack, Socket-Reuse.
Status V13.1: TCP-Blocking in server/Network.cpp.
Empfohlene Bibliothek: ENet oder GameNetworkingSockets (Valve, BSD/MIT).
Neue Struktur:
server/network/
├── UdpSocket.h/cpp          # Platform-Abstraction
├── NetworkConfig.h          # MTU, Bandwidth-Limits
└── Platform/
├── UdpSocketWin.cpp     # IOCP
└── UdpSocketLinux.cpp   # epoll
Akzeptanzkriterien:
•  [ ] 1.000 Packets/sec pro Client
•  [ ] <1ms Socket-Overhead
•  [ ] Graceful Handling von 1000+ gleichzeitigen Sockets
AP-33: Zuverlässiges UDP (Reliability)
Phase: 3 | Modul: server/network/ | Abhängigkeiten: AP-32
Ziel: Sequenznummern, SACK, RTT-Schätzung (Jacobson/Karn), Fragmentierung >MTU.
Status V13.1: TCP garantiert Zuverlässigkeit — bei UDP muss das selbst gebaut werden.
Komponenten:
struct ReliableChannel {
uint16_t localSequence;
uint16_t remoteSequence;
uint32_t ackBitmap;  // SACK: 32 acks in einem Paket
std::deque<Packet> sendQueue;
std::deque<Packet> recvQueue;
float rtt;           // Smoothed RTT
float rttVariance;   // Jacobson/Karn

};
Akzeptanzkriterien:
•  [ ] 0% Packet-Loss bei <5% echtem Loss
•  [ ] RTT-Schätzung ±10ms genau
•  [ ] Fragmentierung: 64KB Pakete zuverlässig
AP-37: World-Snapshot-Builder
Phase: 3 | Modul: server/network/ | Abhängigkeiten: AP-20, AP-33
Ziel: Serialisierung des ECS-World-States, Delta-Kompression gegenüber vorherigem ACK'd Snapshot.
Status V13.1: Manuelle ByteBuffer-Pakete pro Entity-Typ.
Neue Architektur:
// server/network/Snapshot.h
struct Snapshot {
uint32_t sequence;
std::vector<EntityState> entities;
std::vector<uint32_t> destroyedEntities;
};
class SnapshotBuilder {
public:
Snapshot Build(const EcsWorld& world, uint32_t baseSequence);
// Delta-Kompression: nur geänderte Komponenten seit baseSequence
};
Akzeptanzkriterien:
•  [ ] Snapshot 10k Entities in <5ms
•  [ ] Delta-Kompression: <10% Bandbreite gegenüber Full-Snapshot
•  [ ] Client-Interpolation: smooth 60fps bei 20Hz Snapshot-Rate
AP-38: Client-Side Interpolation
Phase: 3 | Modul: client/ | Abhängigkeiten: AP-37
Ziel: Snapshot-Buffer (min. 3 Snaps), Entity-Interpolation, Extrapolation bei Loss.
Status V13.1: Einfaches Lerp in client/ClientTick.cpp.
Neue Logik:
// client/Interpolation.h
class Interpolator {
static constexpr size_t BUFFER_SIZE = 32;
std::array<Snapshot, BUFFER_SIZE> snapshotBuffer;
size_t writeIndex = 0;
public:
void AddSnapshot(const Snapshot& snap);
void Interpolate(float renderTime, Entity& out);
// Extrapolation: wenn kein Snapshot verfügbar, velocity-based
};
Akzeptanzkriterien:
•  [ ] Jitter <5ms visuell wahrnehmbar
•  [ ] Extrapolation: max 200ms, dann Freeze
•  [ ] Bandbreite: <50KB/s pro Client bei 10k Entities
AP-42: Multi-Threaded Server
Phase: 3 | Modul: server/ | Abhängigkeiten: AP-32, AP-37
Ziel: Netzwerk-Thread (IOCP/epoll), Simulations-Thread(s), DB-Worker-Thread, lock-freie Message-Queues.
Status V13.1: Single-Threaded Loop in server/Server.cpp.
Neue Architektur:
Threads:
├── NetworkThread:    UDP Send/Recv, Connection-Management
├── SimThread:        ECS-System-Execution (fixed 20Hz/60Hz)
├── DbThread:         PostgreSQL-Queries, Redis-Cache
└── MainThread:       Orchestration, Metrics
Akzeptanzkriterien:
•  [ ] 1.000 CCU bei <20ms Server-Tick
•  [ ] 0 Lock-Contention in Hotpaths (Tracy-Proof)
•  [ ] Graceful Shutdown: alle Queues leer
AP-45: Authentication-Service
Phase: 3 | Modul: server/auth/ | Abhängigkeiten: Keine (ersetzt V13.1-Fake)
Ziel: JWT-basierte Auth, echte Argon2id (libsodium), PostgreSQL-Account-DB, Rate-Limiting.
Status V13.1: Argon2IdHash() in core/GameSystems.cpp ist kryptographisch unsicher (eigene Hash-Funktion).
Änderungen:
// server/auth/AuthService.h
class AuthService {
public:
[[nodiscard]] std::expected<Token, AuthError> Login(
std::string_view username,
std::string_view password);
[[nodiscard]] bool VerifyToken(std::string_view token);

private:
// libsodium: crypto_pwhash_argon2id
std::string HashPassword(std::string_view password, std::span<uint8_t> salt);
};
Akzeptanzkriterien:
•  [ ] Argon2id: t_cost=3, m_cost=65536, parallelism=4
•  [ ] JWT: RS256, 15min Access + 7d Refresh
•  [ ] Rate-Limiting: 5 Versuche/Min pro IP
AP-47: NavMesh-Generation
Phase: 4 | Modul: server/ai/ | Abhängigkeiten: AP-20 (ECS für Positionen)
Ziel: Recast-Integration, automatisches Bake aus Terrain-Daten, Tile-basiert für große Welten.
Status V13.1: Keine AI — Monster stehen an Spawn-Punkten.
Empfohlene Bibliothek: Recast + Detour (Zlib).
Akzeptanzkriterien:
•  [ ] NavMesh aus V13.1-Terrain generiert in <5s
•  [ ] Tile-Größe: 256x256 World-Units
•  [ ] Off-Mesh-Links: Sprungpunkte, Türen
AP-48: Pathfinding (Crowd)
Phase: 4 | Modul: server/ai/ | Abhängigkeiten: AP-47
Ziel: Detour-Crowd, Pfad-Smoothing, async. Pfadberechnung via Job-System, Invalidierung bei Terrain-Änderungen.
Akzeptanzkriterien:
•  [ ] 500 Agents mit Crowd-Avoidance @ 60Hz
•  [ ] Pfad-Invalidierung: <1ms bei Terrain-Brush
•  [ ] Async: Pfadberechnung nicht blockierend
AP-49: Behavior Tree Runtime
Phase: 4 | Modul: server/ai/ | Abhängigkeiten: AP-48
Ziel: BT-Runtime (Custom oder behaviortree.cpp), Node-Typen: Sequence, Selector, Parallel, Decorator.
Empfohlene Bibliothek: BehaviorTree.CPP (MIT) oder Custom-Implementierung.
Akzeptanzkriterien:
•  [ ] 10.000 BT-Ticks/ms
•  [ ] Blackboard: Shared Memory zwischen Nodes
•  [ ] Hot-Reload: BT-XML ändern ohne Restart
AP-53: Combat-System (Erweitert)
Phase: 4 | Modul: server/gameplay/ | Abhängigkeiten: AP-20 (ECS)
Ziel: Hitbox-System (Box/Sphere/Capsule), Combo-System, Damage-Calculation (Armor, Resistance, Critical).
Status V13.1: Einfacher HP-Abzug in server/PacketHandler.cpp.
Neue Komponenten:
// core/ecs/Components.h
struct Hitbox { enum Type { Box, Sphere, Capsule }; /* ... */ };
struct CombatStats { float armor, resistance, critChance, critMultiplier; };
struct ComboState { uint32_t currentCombo; float windowRemaining; };
Akzeptanzkriterien:
•  [ ] Hitbox-Test: <0.1ms für 1000 Entities
•  [ ] Combo-System: 3-Hit-Combo mit Timing-Fenster
•  [ ] Damage-Formel: Armor-Mitigation, Resistance-Flat, Crit-Roll
AP-64: Lua-Runtime-Integration
Phase: 4 | Modul: scripting/ | Abhängigkeiten: AP-20 (ECS-Zugriff)
Ziel: Lua 5.4 + sol3, ECS-Component-Zugriff, Event-System-Registrierung, Sandboxing.
Empfohlene Bibliothek: sol3 (MIT) für modernes C++17/20/23 Binding.
Akzeptanzkriterien:
•  [ ] Lua-Script kann Entity erstellen/lesen/schreiben
•  [ ] Event-Subscription aus Lua
•  [ ] Sandbox: kein File-IO, kein Network-IO aus Lua
•  [ ] Hot-Reload: Script-Änderung ohne Neustart
PRIORITÄT: HOCH (Wichtig für Produktionsreife)
AP	Name	Phase	Abhängigkeiten	Status V13.1
AP-02	Render-Pass & Framebuffer	1	AP-01	Neu
AP-03	Resource-Manager	1	AP-01	Ersetzt assetRegistry
AP-04	Deferred Shading Pipeline	1	AP-02, AP-03	Ersetzt Toon-Shader
AP-05	PBR-Material-System	1	AP-04	Ersetzt RegisterMaterial
AP-06	CSM	1	AP-04	Neu
AP-07	Post-Processing	1	AP-04	Neu
AP-09	Skeletal Mesh	1	AP-01	Neu
AP-10	Instanced Rendering	1	AP-01	Ersetzt brute-force
AP-13	Math-Library (SIMD)	1	Keine	Ersetzt Vector3
AP-14	Memory-Allocators	1	Keine	Neu
AP-15	File-IO & VFS	1	Keine	Ersetzt std::ifstream
AP-16	Reflection & Serialization	1	Keine	Neu
AP-18	Platform-Abstraction	1	Keine	Erweitert GLFW
AP-23	World-Migration V12→ECS	2	AP-20–22	Adapter-Layer
AP-24	ECS-Unit-Tests	2	AP-20–22	GoogleTest
AP-25	ECS-Chunk-Allocator	2	AP-20	16KB-Chunks
AP-26	Component-Serializer	2	AP-20, AP-16	Netzwerk-Delta
AP-28	Fibers/Job-Queue	2	AP-14	Work-Stealing
AP-29	Parallel-For	2	AP-28	SIMD-Iteration
AP-30	Sync-Primitive	2	AP-28	Fiber-Semaphore
AP-34	Connection-Management	3	AP-32	Ersetzt accept()
AP-35	Packet-Priorisierung	3	AP-33	Multi-Queue
AP-36	Crypto-Layer	3	AP-32	DTLS-ähnlich
AP-39	Entity-Priority & LOD-Net	3	AP-37	Distanz-basiert
AP-40	Interest-Management Rewamp	3	AP-37	Ersetzt O(n) AOI
AP-41	Network-Profiler	3	AP-37	Tracy-Integration
AP-43	Zone-Server-Architektur	3	AP-42	Multi-Prozess
AP-44	Instancing-System	3	AP-43	Dynamisch
AP-46	Game-DB (PostgreSQL)	3	AP-42	Ersetzt SQLite
AP-50	AI-Behavior-Library	4	AP-49	Patrol, Chase, etc.
AP-51	Aggro & Threat-System	4	AP-49	Threat-Table
AP-54	Loot-System	4	AP-53	Loot-Tables
AP-55	Progression-System	4	AP-53	XP, Skill-Trees
AP-59	Gilden-System	4	AP-46	PostgreSQL
AP-60	PvP-System	4	AP-53	Duell, BG
AP-61	Gruppen-System	4	AP-53	Party, Raid
AP-65	Scriptable-Quest-System	4	AP-64	Lua-Quests
AP-66	Scriptable-AI-Behaviors	4	AP-64, AP-49	Lua-BT-Nodes
AP-68	Editor-Windowing (Docking)	5	Keine	ImGui-Docking
AP-69	3D-Viewport mit Gizmos	5	AP-01	ImGuizmo
AP-70	Entity-Outliner & Inspector	5	AP-20	Erweitert V13.1
AP-75	Asset-Importer	5	Keine	assimp, glTF
AP-76	Asset-Cooking	5	AP-75	meshoptimizer
AP-77	Texture-Pipeline	5	AP-75	BC7/ASTC
AP-80	GitHub-Actions CI/CD	5	AP-17	Build-Matrix
AP-81	Container-Deployment	5	AP-80	Docker
AP-85	GPU-Driven-Rendering	6	AP-01	Mesh-Shader
AP-86	Occlusion Culling	6	AP-85	OIDN
AP-87	Streaming & Loading	6	AP-85	Async-Sektoren
AP-88	Server-Performance	6	AP-42	Spatial-Hash
AP-90	Linux-Client	6	AP-01	Steam-Deck
AP-94	Automated-Testing	6	AP-80	>80% Coverage
AP-95	Load-Testing	6	AP-42	1000+ Bots
PRIORITÄT: MITTEL (Komfort & Produktivität)
AP	Name	Phase	Anmerkung
AP-08	GPU-Partikel	1	Compute-Shader
AP-11	Dynamic Resolution	1	FSR 2.x
AP-12	Debug-Render-Pass	1	Wireframe
AP-19	Profiling	1	Tracy
AP-27	String-Interning	2	Hash-IDs
AP-31	System-Parallelisierung	2	Auto-parallel
AP-52	Group-AI	4	Formationen
AP-56	Crafting	4	Rezepte
AP-57	Auction House	4	Wirtschaft
AP-58	Mounts	4	Transport
AP-62	Friends & Block	4	Sozial
AP-63	Chat-Erweitert	4	Kanäle
AP-67	Visual-Scripting	4	Node-Graph
AP-71	Terrain-Editor	5	3D-Paint
AP-72	VS-Editor	5	imnodes
AP-73	Animation-Editor	5	State-Machine
AP-74	Database-Editor	5	Tabellen
AP-78	Asset-Browser	5	Thumbnails
AP-79	Hot-Reload	5	File-Watcher
AP-82	Monitoring	5	Prometheus
AP-83	Deployment-Auto	5	Blue/Green
AP-89	Memory-Optimierung	6	Budgets
AP-91	macOS-Port	6	MoltenVK
AP-93	Controller	6	Accessibility
AP-96	Closed-Beta	6	100-500 Tester
PRIORITÄT: NIEDRIG (Nice-to-Have)
AP	Name	Phase	Anmerkung
AP-58	Mounts & Transport	4	Niedrig in Roadmap
AP-74	Database-Editor	5	Manuell geht auch
AP-84	Crash-Reporter	5	Breakpad
AP-92	Konsolen-Vorbereitung	6	Mittel in Tabelle, aber niedrigere Priorität als Linux
AP-97	Polishing-Pass	6	Visuelles Final
Zusammenfassung: Empfohlene Reihenfolge für V13.1
Woche 1–2: Fundament (keine Abhängigkeiten)
1.  AP-17 — CMake + vcpkg professionalisieren
2.  AP-13 — SIMD Math-Library (Header-only, glm oder custom)
3.  AP-14 — Memory-Allocators (Pool, Stack)
Woche 3–6: Rendering-Kern
4.  AP-01 — Vulkan-Init (mit vk-bootstrap)
5.  AP-02 — Render-Pass-Abstraktion
6.  AP-03 — Resource-Manager
Woche 7–10: ECS-Kern
7.  AP-20 — Archetype Storage
8.  AP-21 — Entity-Manager & Queries
9.  AP-22 — System-Registry
10.  AP-23 — Migration Adapter (V13.1 → ECS)
Woche 11–14: Netzwerk-Kern
11.  AP-32 — UDP-Socket-Layer
12.  AP-33 — Reliable UDP
13.  AP-37 — Snapshot-Builder
14.  AP-38 — Client-Interpolation
Woche 15–18: Server-Skalierung
15.  AP-42 — Multi-Threaded Server
16.  AP-45 — Auth-Service (libsodium)
17.  AP-46 — PostgreSQL-Migration
Woche 19+: Gameplay & AI
18.  AP-47–49 — NavMesh, Pathfinding, Behavior Trees
19.  AP-53 — Combat-System
20.  AP-64 — Lua-Integration
----