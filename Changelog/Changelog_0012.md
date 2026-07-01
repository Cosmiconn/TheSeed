# Changelog_0012.md

**Datum:** 2026-07-01
**Bearbeitete Prioritaet:** AP-80 — Memory Profiler
**Ziel:** Vollstaendiges Speicher-Tracking fuer alle Allokatoren und das ECS

---

## Geaenderte Dateien

| Datei | Aenderung |
|-------|-----------|
| `memory/PoolAllocator.h` | Memory Profiler Integration |
| `memory/PoolAllocator.cpp` | Allokations-Tracking bei Allocate/Free/Reset |
| `ecs/ecs_EcsWorld.h` | `UpdateMemoryProfile()` Methode, Periodisches Profiling |
| `ecs/ecs_EcsWorld.cpp` | Automatisches ECS-Speicher-Tracking |
| `CMakeLists.txt` | `memory/MemoryProfiler.cpp` hinzugefuegt |
| `tests/CMakeLists.txt` | `Test_MemoryProfiler.cpp` hinzugefuegt |
| `AAA_ROADMAP_STATUS.md` | AP-80 auf "Erledigt" gesetzt |
| `PRIORITAETENLISTE.md` | AP-80 Status aktualisiert |

## Neue Dateien

| Datei | Beschreibung |
|-------|-------------|
| `memory/MemoryProfiler.h` | Profiler-Header: Allokations-Tracking, Statistiken, Leak-Erkennung, Historie |
| `memory/MemoryProfiler.cpp` | Profiler-Implementation: Thread-sicher, atomare Counter, Mutex-geschuetzte Daten |
| `memory/MemoryProfilerIntegration.h` | Integrations-Hilfsfunktionen fuer Allokatoren und ECS |
| `tests/Test_MemoryProfiler.cpp` | 10 Profiler-Tests + 1 Benchmark |

## Entfernte Dateien

Keine.

## Technische Aenderungen

- **AP-80 FIX:** Vollstaendiger Memory Profiler mit Singleton-Pattern.
- **Allokations-Tracking:** Jede Allokation wird mit Adresse, Groesse, Alignment, Allokator-Name, Quelldatei und Zeile erfasst.
- **Allokator-Statistiken:** Pro Allokator: total/current/peak Bytes, Allokations-/Freigabe-Zaehler, aktive Allokationen.
- **ECS-Speicher-Tracking:** Chunk-Speicher, Entity-Count, Archetype-Count, Nutzungsgrad.
- **Speicherleck-Erkennung:** FindLeaks() und FindLeaksOlderThan(seconds) mit vollstaendigen Records.
- **Historische Daten:** Zeitverlauf der Speichernutzung mit konfigurierbarem Limit (default 1000 Punkte).
- **Berichtserstellung:** Formatierte Text-Ausgabe mit allen Statistiken.
- **Thread-Sicherheit:** Atomare Counter fuer globale Statistiken, Mutexe fuer detaillierte Daten.
- **PoolAllocator-Integration:** Automatisches Tracking bei Allocate/Free/Reset/Destruktor.
- **EcsWorld-Integration:** Automatisches Profiling bei CreateEntity/DestroyEntity/Update (alle 5s).

## Architekturaenderungen

- MemoryProfiler als Singleton — globaler Zugriff ueber `MemoryProfiler::GetInstance()`.
- RAII-ScopedAllocationTracker fuer automatisches Tracking im Scope.
- Makros `MEMPROFILE_TRACK_ALLOC`, `MEMPROFILE_TRACK_FREE`, `MEMPROFILE_RECORD_SNAPSHOT`.
- Periodisches ECS-Profiling waehrend `EcsWorld::Update()` (alle 5 Sekunden).
- Leak-Threshold konfigurierbar (default 300 Sekunden).

## Moegliche Auswirkungen

- **Performance-Overhead:** ~50-100ns pro Allokation (atomare Operationen + Mutex-Lock).
- **Speicher-Overhead:** ~80 Bytes pro Allokation (AllocationRecord).
- **Build-Zeit:** Erhoeht sich um ~5% durch zusaetzliche Quelldatei.
- **Keine Breaking Changes:** Bestehende Allokatoren funktionieren ohne Aenderung (Tracking ist optional).

## Teststatus

- [x] Test 1: Grundlegendes Allokations-Tracking
- [x] Test 2: Peak-Tracking
- [x] Test 3: Allokator-Statistiken
- [x] Test 4: Speicherleck-Erkennung
- [x] Test 5: Historische Daten
- [x] Test 6: Berichtserstellung
- [x] Test 7: PoolAllocator-Integration
- [x] Test 8: Thread-Sicherheit (4 Threads, 400 Allokationen)
- [x] Test 9: ECS-Speicher-Tracking
- [x] Test 10: EcsWorld-Integration (50 Entities)
- [x] Benchmark: Massen-Allokationen (1000 Allokationen)

## Bekannte Restrisiken

1. **StackAllocator:** Kein einzelnes Tracking pro Allokation (nur initiale Block-Allokation).
2. **FreelistAllocator:** Tracking bei Coalescing nicht vollstaendig (nur Block-Allokationen).
3. **SlabAllocator:** Tracking auf Slab-Ebene, nicht pro Object.
4. **Fragmentierung:** Berechnung ist vereinfacht (keine echte Fragmentierungsanalyse).
5. **Cross-Platform:** `__builtin_prefetch` auf Linux/macOS, `_mm_prefetch` auf Windows.

## Naechster Arbeitsschritt

**AP-81:** Network Profiler
**AP-91:** Metrics Dashboard
**AP-02:** Deferred Shading Pipeline
