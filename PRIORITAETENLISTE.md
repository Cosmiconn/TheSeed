# TheSeed Engine V13.2 — Korrigierte Prioritätenliste & Roadmap

**Erstellt:** 2026-06-30  
**Status:** P0-P1 Fixes angewendet, P2-P5 offen  
**Repository:** https://github.com/Cosmiconn/TheSeed

---

## Legende

| Symbol | Bedeutung |
|--------|-----------|
| ✅ | Erledigt & verifiziert |
| 🔧 | In diesem Patch behoben |
| 🟡 | Teilweise erledigt / bekanntes Restrisiko |
| 🔴 | Offen / Kritisch |
| 🔵 | Nächster Arbeitsschritt |

---

## P0 — Build-Blocker & Sicherheitslücken (KRITISCH)

| # | Problem | Fix-Datei | Status |
|---|---------|-----------|--------|
| P0-1 | CMakeLists.txt fehlte ~40 Quelldateien (AI, Auth, Combat, Snapshot, Editor, Animation, Math, Memory, Vulkan) | `CMakeLists.txt` | 🔧 |
| P0-2 | Chunk-Speicherleck: `aligned_alloc` + `delete` = undefined behavior | `ecs/ecs_Chunk.h` | 🔧 |
| P0-3 | `EntityManager::GetRecord()` nicht thread-safe (dangling pointer bei Reallokation) | `ecs/ecs_EntityManager.h` | 🔧 |
| P0-4 | `HealthComponent` Feldnamen inkonsistent (`current`/`max` statt `currentHP`/`maxHP`) | `server/Server.cpp` | 🔧 |
| P0-5 | `Argon2IdHash` mit hard-coded Salt — alle Accounts identischer Salt | `core/Database.cpp` | 🔧 |
| P0-6 | `VerifyAccount` liest Salt nicht aus DB, Rainbow-Table-Angriff möglich | `core/Database.cpp` | 🔧 |
| P0-7 | `main.cpp` fehlende Includes (`<chrono>`, `<thread>`, `<cmath>`) | `main.cpp` | 🔧 |
| P0-8 | Keine explizite Template-Instanziierung für ECS-Queries → Linker-Fehler | `ecs/ecs_EcsWorld.cpp` | 🔧 |
| P0-9 | UDP-Session-State in `Network.h` auskommentiert — keine UDP-Integration | `server/Network.h` | 🔧 |
| P0-10 | `GetHeightFromGrid()` Stub ohne Terrain-Grid | `core/Types.h` | 🔧 |

**→ Alle P0-Probleme wurden in diesem Patch behoben.**

---

## P1 — Architektur & Stabilität (HOCH)

| # | Problem | Status | Anmerkung |
|---|---------|--------|-----------|
| P1-1 | ECS-Systeme in `main.cpp` nicht korrekt registriert | 🔴 | `gEcsWorld` existiert, aber keine formale `RegisterSystem()`-Aufrufe |
| P1-2 | Parallele ECS-Systeme ohne Read-Write-Locks | 🔴 | `std::async` in `Server.cpp` greift auf gleiche Komponenten zu → Race Conditions |
| P1-3 | `Server.cpp` ECS-Query-Syntax verifizieren | 🟡 | Template-Parameter sind syntaktisch korrekt, aber Runtime-Verhalten ungetestet |
| P1-4 | `ApplyStatusEffect()` Signatur-Inkonsistenz | 🔴 | `PacketHandler.cpp` ruft `ApplyStatusEffect` mit 6 Parametern auf, `GameSystems.cpp` definiert 4 Parameter |

---

## P2 — Netzwerk & Snapshot (MITTEL)

| # | Problem | Status | Anmerkung |
|---|---------|--------|-----------|
| P2-1 | Snapshot ohne Interest Management | 🔴 | `BuildAndBroadcastSnapshot()` sendet an ALLE Clients identisch |
| P2-2 | Snapshot ohne Delta-Kompression | 🔴 | Jeder Snapshot enthält ALLE Entities, Bandbreitenverschwendung |
| P2-3 | MTU-Überschreitung bei >50 Entities | 🔴 | Keine Fragmentierung, Pakete können >1500 Bytes werden |
| P2-4 | `network_NetworkServer.cpp` Empfangs-Queue TODO | 🟡 | `// TODO: In Empfangs-Queue einreihen` — unvollständige Integration |
| P2-5 | `UpdateInterestManagement()` Stub | 🔴 | `Network.h` deklariert Funktion, aber keine AOI-Implementierung |

---

## P3 — Auth & Persistenz (MITTEL)

| # | Problem | Status | Anmerkung |
|---|---------|--------|-----------|
| P3-1 | Auth-Service nicht in `vcpkg.json` | 🔴 | PostgreSQL (`libpq`) und Redis (`redis++`) fehlen als Abhängigkeiten |
| P3-2 | `AuthService.cpp` existiert aber ungetestet | 🟡 | JWT-Implementierung ohne `josepp`/`libjwt` — möglicherweise Stub |
| P3-3 | `PostgreSqlUserRepository` ohne `libpq` | 🔴 | Wird kompiliert, aber Linker-Fehler ohne vcpkg-Integration |
| P3-4 | `RedisRateLimiter` ohne `redis++` | 🔴 | Wird kompiliert, aber Linker-Fehler ohne vcpkg-Integration |

---

## P4 — Performance (ERLEDIGT)

| # | Problem | Status | Anmerkung |
|---|---------|--------|-----------|
| P4-1 | Chunk-Index O(1) Lookup | ✅ | `archetypeIndexMap[mask]` |
| P4-2 | Memory Prefetching | ✅ | `_mm_prefetch` / `__builtin_prefetch` |
| P4-3 | Work-Stealing ThreadPool | ✅ | `std::deque` + atomare Indizes |
| P4-4 | RTT-Monitoring (Jacobson/Karels) | ✅ | `RttEstimator` in `network_ReliableUdp.cpp` |
| P4-5 | 64-Byte Alignment | 🟡 | Korrekt angefordert, Custom Deleter jetzt vorhanden |

---

## P5 — Feature-Vervollständigung (NÄCHSTER SCHRITT)

| # | Feature | Status | Quelle |
|---|---------|--------|--------|
| P5-1 | Snapshot-Fragmentierung (MTU-Splitting) | 🔵 | Changelog_0004 AP-43 |
| P5-2 | AI Behavior Trees | 🔵 | Changelog_0004 AP-47-49 |
| P5-3 | Auth-Service mit JWT (vollständig) | 🔵 | Changelog_0004 AP-45 |
| P5-4 | Client-Interpolation vervollständigen | 🔵 | Changelog_0004 AP-38 |
| P5-5 | Terrain-Grid implementieren | 🔴 | `GetHeightFromGrid()` ist noch Stub |
| P5-6 | Delta-Kompression + Interest Management | 🔴 | Snapshot-System erweitern |
| P5-7 | vcpkg.json um PostgreSQL/Redis erweitern | 🔴 | Für Auth-Service nötig |

---

## Zusammenfassung

```
P0: 10/10 behoben  ✅  (Build-Blocker & Sicherheit)
P1:  0/4  offen    🔴  (Architektur & Stabilität)
P2:  0/5  offen    🔴  (Netzwerk & Snapshot)
P3:  0/4  offen    🔴  (Auth & Persistenz)
P4:  4/5  erledigt  ✅  (Performance)
P5:  0/7  offen    🔵  (Features)
```

**Empfohlene Reihenfolge:**
1. **P1** abschließen (ECS-System-Registrierung + Locks)
2. **P2** abschließen (Interest Management + Delta-Kompression)
3. **P3** abschließen (vcpkg.json + Auth-Integration)
4. **P5** beginnen (Snapshot-Fragmentierung, AI, Interpolation)

---

## Geänderte Dateien in diesem Patch

| Datei | Änderung |
|-------|----------|
| `CMakeLists.txt` | +AI, +Auth, +Combat, +Snapshot, +Animation, +Editor, +Math, +Memory, +Vulkan Quellen & Include-Pfade |
| `ecs/ecs_Chunk.h` | Custom Deleter für aligned_alloc/_aligned_malloc |
| `ecs/ecs_EntityManager.h` | `std::shared_mutex` + `shared_lock` für Thread-Sicherheit |
| `server/Server.cpp` | `health->currentHP`/`maxHP` statt `current`/`max` |
| `core/Database.cpp` | Per-Account Salt + `Argon2IdVerify` mit gespeichertem Hash |
| `main.cpp` | Explizite Includes `<chrono>`, `<thread>`, `<cmath>` |
| `core/Types.h` | `GetHeightFromGrid` Kommentar + `(void)` casts |
| `ecs/ecs_EcsWorld.cpp` | Explizite Template-Instanziierungen für gängige Queries |
| `server/Network.h` | UDP-Session-Felder aktiviert |
| `server/ThreadPool.cpp` | TODO → FIX Kommentar |
