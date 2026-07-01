# TheSeed Engine V13.2 — Korrigierte Prioritaetenliste & Roadmap

**Erstellt:** 2026-06-30
**Status:** P0-P5 Fixes angewendet, AP-78 + AP-80 erledigt
**Repository:** https://github.com/Cosmiconn/TheSeed

---

## Legende

| Symbol | Bedeutung |
|--------|-----------|
| ✅ | Erledigt & verifiziert |
| 🔧 | In diesem Patch behoben |
| 🟡 | Teilweise erledigt / bekanntes Restrisiko |
| 🔴 | Offen / Kritisch |
| 🔵 | Naechster Arbeitsschritt |

---

## P0 — Build-Blocker & Sicherheitsluecken (KRITISCH)

| # | Problem | Fix-Datei | Status |
|---|---------|-----------|--------|
| P0-1 | CMakeLists.txt fehlte ~40 Quelldateien | `CMakeLists.txt` | ✅ |
| P0-2 | Chunk-Speicherleck: `aligned_alloc` + `delete` | `ecs/ecs_Chunk.h` | ✅ |
| P0-3 | `EntityManager::GetRecord()` nicht thread-safe | `ecs/ecs_EntityManager.h` | ✅ |
| P0-4 | `HealthComponent` Feldnamen inkonsistent | `server/Server.cpp` | ✅ |
| P0-5 | `Argon2IdHash` mit hard-coded Salt | `core/Database.cpp` | ✅ |
| P0-6 | `VerifyAccount` liest Salt nicht aus DB | `core/Database.cpp` | ✅ |
| P0-7 | `main.cpp` fehlende Includes | `main.cpp` | ✅ |
| P0-8 | Keine explizite Template-Instanziierung | `ecs/ecs_EcsWorld.cpp` | ✅ |
| P0-9 | UDP-Session-State in `Network.h` auskommentiert | `server/Network.h` | ✅ |
| P0-10 | `GetHeightFromGrid()` Stub | `core/Types.h` | ✅ |

**→ Alle P0-Probleme wurden behoben.**

---

## P1 — Architektur & Stabilitaet (HOCH)

| # | Problem | Status | Anmerkung |
|---|---------|--------|------------|
| P1-1 | ECS-Systeme in `main.cpp` nicht korrekt registriert | ✅ | `RegisterEcsSystems()` implementiert |
| P1-2 | Parallele ECS-Systeme ohne Read-Write-Locks | ✅ | `componentMutex` in `EcsWorld` |
| P1-3 | `Server.cpp` ECS-Query-Syntax verifizieren | ✅ | `ecs::` Namespace korrekt |
| P1-4 | `ApplyStatusEffect()` Signatur-Inkonsistenz | ✅ | 6-Parameter-Signatur ueberall |

**→ Alle P1-Probleme wurden behoben.**

---

## P2 — Netzwerk & Snapshot (MITTEL)

| # | Problem | Status | Anmerkung |
|---|---------|--------|------------|
| P2-1 | Snapshot ohne Interest Management | ✅ | Spatial Hash implementiert |
| P2-2 | Snapshot ohne Delta-Kompression | ✅ | `BuildDeltaSnapshot()` implementiert |
| P2-3 | MTU-Ueberschreitung bei >50 Entities | ✅ | `SendFragmented()` implementiert |
| P2-4 | `network_NetworkServer.cpp` Empfangs-Queue | ✅ | `QueueReliablePacket()` implementiert |
| P2-5 | `UpdateInterestManagement()` Stub | ✅ | Spatial-Hash-basiert |

**→ Alle P2-Probleme wurden behoben.**

---

## P3 — Auth & Persistenz (MITTEL)

| # | Problem | Status | Anmerkung |
|---|---------|--------|------------|
| P3-1 | Auth-Service nicht in `vcpkg.json` | ✅ | Features `auth-full`, `auth-postgresql`, `auth-redis` |
| P3-2 | `AuthService.cpp` ungetestet | ✅ | JWT + Argon2id vollstaendig |
| P3-3 | `PostgreSqlUserRepository` ohne `libpq` | ✅ | `libpq` via vcpkg |
| P3-4 | `RedisRateLimiter` ohne `redis++` | ✅ | `hiredis` via vcpkg |

**→ Alle P3-Probleme wurden behoben.**

---

## P4 — Performance (ERLEDIGT)

| # | Problem | Status | Anmerkung |
|---|---------|--------|------------|
| P4-1 | Chunk-Index O(1) Lookup | ✅ | `archetypeIndexMap[mask]` |
| P4-2 | Memory Prefetching | ✅ | `_mm_prefetch` / `__builtin_prefetch` |
| P4-3 | Work-Stealing ThreadPool | ✅ | Lock-Free Queue |
| P4-4 | RTT-Monitoring (Jacobson/Karels) | ✅ | `RttEstimator` |
| P4-5 | 64-Byte Alignment | ✅ | `aligned_alloc` + Custom Deleter |
| AP-80 | Memory Profiler | ✅ | `MemoryProfiler.h/cpp` |

**→ Alle P4-Probleme + AP-80 wurden behoben.**

---

## P5 — Feature-Vervollstaendigung (ERLEDIGT)

| # | Feature | Status | Quelle |
|---|---------|--------|--------|
| P5-1 | Snapshot-Fragmentierung (MTU-Splitting) | ✅ | `SendFragmented()` |
| P5-2 | AI Behavior Trees | ✅ | `AIBehavior.h/cpp` |
| P5-3 | Auth-Service mit JWT (vollstaendig) | ✅ | `AuthService.cpp` |
| P5-4 | Client-Interpolation vervollstaendigen | ✅ | `Interpolation.cpp` |
| P5-5 | Terrain-Grid implementieren | ✅ | `TerrainGrid.h` |
| P5-6 | Delta-Kompression + Interest Management | ✅ | `Snapshot.cpp` |
| P5-7 | vcpkg.json um PostgreSQL/Redis erweitern | ✅ | `vcpkg.json` |

**→ Alle P5-Probleme wurden behoben.**

---

## AP-78 — Integration Tests (ERLEDIGT)

| # | Test | Status | Beschreibung |
|---|------|--------|-------------|
| IT-1 | Server-Start und Client-Verbindung | ✅ | Socket-Initialisierung |
| IT-2 | ECS-Entity Lifecycle | ✅ | Create, Add, Get, Remove, Destroy |
| IT-3 | Spatial Hash | ✅ | Insert, Query, Remove, Update, Clear |
| IT-4 | Paket-Serialisierung | ✅ | Move, Combat, Chat Pakete |
| IT-5 | EventBus | ✅ | Publish/Subscribe, async |
| IT-6 | ThreadPool | ✅ | Tasks, Work-Stealing, Performance |
| IT-7 | Client-Interpolation | ✅ | Snapshot-History, Interpolation |
| IT-8 | Auth-Flow | ✅ | SQLite-Backend, Registrierung, Login |
| IT-9 | Delta-Kompression | ✅ | Snapshot-Vergleich, MTU |
| IT-10 | Vollstaendiger Spielablauf | ✅ | ECS, Movement, Combat, Query |

**→ AP-78 erledigt. 30+ Tests, 7 Benchmarks.**

---

## AP-80 — Memory Profiler (ERLEDIGT)

| # | Feature | Status | Beschreibung |
|---|---------|--------|-------------|
| MP-1 | Allokations-Tracking | ✅ | Adresse, Groesse, Alignment, Quelle |
| MP-2 | Allokator-Statistiken | ✅ | Per-Allocator: total/current/peak |
| MP-3 | ECS-Speicher-Tracking | ✅ | Chunks, Entities, Archetypes |
| MP-4 | Speicherleck-Erkennung | ✅ | FindLeaks(), FindLeaksOlderThan() |
| MP-5 | Historische Daten | ✅ | Zeitverlauf, konfigurierbares Limit |
| MP-6 | Berichtserstellung | ✅ | Formatierte Text-Ausgabe |
| MP-7 | Thread-Sicherheit | ✅ | Atomare Counter, Mutexe |
| MP-8 | PoolAllocator-Integration | ✅ | Automatisches Tracking |
| MP-9 | EcsWorld-Integration | ✅ | Periodisches Profiling |
| MP-10 | Tests | ✅ | 10 Tests + 1 Benchmark |

**→ AP-80 erledigt. 47+ Tests, 8 Benchmarks.**

---

## Zusammenfassung

```
P0:  10/10 behoben ✅ (Build-Blocker & Sicherheit)
P1:   4/ 4 behoben ✅ (Architektur & Stabilitaet)
P2:   5/ 5 behoben ✅ (Netzwerk & Snapshot)
P3:   4/ 4 behoben ✅ (Auth & Persistenz)
P4:   6/ 6 behoben ✅ (Performance + Memory Profiler)
P5:   7/ 7 behoben ✅ (Features)
AP-78: 10/10 bestanden ✅ (Integration Tests)
AP-80: 10/10 bestanden ✅ (Memory Profiler)
```

**Empfohlene Reihenfolge:**
1. **AP-81** Network Profiler
2. **AP-91** Metrics Dashboard
3. **AP-02** Deferred Shading Pipeline

---

## Geaenderte Dateien in diesem Patch (Changelog_0012)

| Datei | Aenderung |
|-------|-----------|
| `memory/MemoryProfiler.h` | NEU: Profiler-Header |
| `memory/MemoryProfiler.cpp` | NEU: Profiler-Implementation |
| `memory/MemoryProfilerIntegration.h` | NEU: Integrations-Hilfsfunktionen |
| `memory/PoolAllocator.h/cpp` | Memory Profiler Tracking |
| `ecs/ecs_EcsWorld.h/cpp` | Automatisches ECS-Profiling |
| `CMakeLists.txt` | MemoryProfiler.cpp hinzugefuegt |
| `tests/CMakeLists.txt` | Test_MemoryProfiler.cpp hinzugefuegt |
| `tests/Test_MemoryProfiler.cpp` | NEU: 10 Profiler-Tests |
| `AAA_ROADMAP_STATUS.md` | AP-80 erledigt |
| `PRIORITAETENLISTE.md` | AP-80 erledigt |
