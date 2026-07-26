# ADR-005: Roadmap-Abweichungen Phase 0 – Architekturentscheidungen

**Status:** Accepted  
**Date:** 2026-07-26  
**Deciders:** TheSeed Core Team  

## Context

Die urspruengliche Roadmap (Monat 1–6, Phase 0) definiert ein striktes
Deliverable-Set fuer das Fundament der TheSeed Engine. Waehrend der
Implementierung haben sich sinnvolle Abweichungen ergeben, die aus
technischen, architektonischen oder pragmatischen Gruenden getroffen
wurden. Dieses ADR dokumentiert alle Abweichungen mit Begruendung.

## Decision Drivers

- Klarheit der API (weniger Dateien = besser verstaendlich)
- Performance (Header-Only wo sinnvoll)
- Wiederverwendbarkeit (Module teilen Komponenten)
- Build-Komplexitaet (weniger Translation Units = schneller Build)
- Testbarkeit (rapidcheck statt handgeschriebener Property-Tests)

## Decisions

### 1. `binary_serializer` → `binary_writer` + `binary_reader`

**Roadmap:** `src/core/serialize/binary_serializer.h/.cpp`  
**Tatsaechlich:** `src/core/serialize/binary_writer.h/.cpp` + `binary_reader.h/.cpp`

**Begruendung:** Die urspruengliche Planung sah einen monolithischen
Serializer vor. Die Aufteilung in Writer und Reader folgt dem
Single-Responsibility-Prinzip und ermoeglicht:
- Unabhaengige Optimierung der Lese-/Schreib-Pfade
- Einfachere Unit-Tests (keine bidirektionale Abhaengigkeit)
- Klarere API: `BinaryWriter::writeX()` vs. `BinaryReader::readX()`

**Konsequenzen:** Keine Breaking Changes – die Public API
(`seed/serialize.h`) abstrahiert die interne Struktur.

---

### 2. `delta_compressor` → `delta.h/.cpp`

**Roadmap:** `src/core/serialize/delta_compressor.h/.cpp`  
**Tatsaechlich:** `src/core/serialize/delta.h/.cpp`

**Begruendung:** Kuerzere Dateinamen ohne Verlust an Semantik.
`DeltaCompressor` ist die Klasse, die in `delta.h` definiert ist.
Die Datei enthaelt zusaetzlich `DeltaDecompressor` als freie Funktion.

**Konsequenzen:** Keine – Naming ist konsistent mit restlichem
Codebase (z.B. `snapshot.h`, `reflection.h`).

---

### 3. `type_registry` in ECS statt Serialize

**Roadmap:** `src/core/serialize/type_registry.h/.cpp`  
**Tatsaechlich:** `src/core/ecs/type_registry.h/.cpp`

**Begruendung:** Das Type-Registry ist primär fuer ECS-Komponenten-
Typisierung notwendig (Archetype-Hashing, Component-IDs). Die
Serialisierung nutzt das Registry als Dependency, nicht umgekehrt.
Die Verschiebung vermeidet zirkulaere Abhaengigkeiten zwischen
ECS und Serialize.

**Konsequenzen:** Serialize-Modul importiert ECS::TypeRegistry.
Das ist akzeptabel, da ECS das zentrale Datenmodell der Engine ist.

---

### 4. `pool_allocator.cpp` entfällt (Template-Only)

**Roadmap:** `src/core/memory/pool_allocator.h` + `pool_allocator.cpp`  
**Tatsaechlich:** Nur `src/core/memory/pool_allocator.h`

**Begruendung:** `PoolAllocator<T, BlockSize>` ist ein Template.
Eine separate `.cpp`-Datei wuerde explizite Instanziierung erfordern
und die Flexibilitaet einschraenken. Header-Only ist fuer Templates
in modernem C++ Standard.

**Konsequenzen:** Keine – Build-Zeit steigt marginal, aber die
API bleibt flexibel fuer beliebige Typen.

---

### 5. Zusaetzliche Module (nicht in Roadmap)

| Modul | Dateien | Begruendung |
|-------|---------|-------------|
| `archetype_manager` | `archetype_manager.h/.cpp` | Kapselt Archetype-Lifecycle (Erstellen, Loeschen, Lookup). Reduziert Komplexitaet in `world.cpp`. |
| `component_traits` | `component_traits.h` | Compile-time Reflection fuer Komponenten (Groesse, Alignment, Konstruktor). Ermoeglicht generische `addComponent<T>()`. |
| `reflection` | `reflection.h/.cpp` | Laufzeit-Reflection fuer Serialisierung. Separat vom ECS-TypeRegistry, da es auch Nicht-ECS-Typen unterstuetzt. |
| `snapshot` | `snapshot.h/.cpp` | ECS-Snapshot fuer Savegames und Netzwerk-Replication. Benoetigt Serialize + ECS, daher eigenes Modul. |
| `memory_system` | `memory_system.h/.cpp` | Singleton-Zugriff auf BlockAllocator und MemoryTracker. Zentralisiert Memory-Management-Konfiguration. |
| `diagnostics` | 7 Dateien | Health-Score, ECS-Validierung, Memory-Checks, Snapshot-Dumps. Notwendig fuer P0-Gate ("24h Leak-free"). |
| `profiling` | 2 Dateien | Tracy-Integration + Assertions. Notwendig fuer Performance-Monitoring und Debug-Builds. |

**Konsequenzen:** Erhoehte Codebase-Groesse (~+15%), aber deutlich
hoehere Wartbarkeit und Testbarkeit. Alle zusaetzlichen Module sind
optional kompilierbar via CMake-Flags.

---

### 6. `math_utils.h/.cpp` hinzugefuegt

**Roadmap:** Nicht explizit aufgefuehrt  
**Tatsaechlich:** `src/core/math/math_utils.h/.cpp`

**Begruendung:** Die Roadmap listet `vec3.h`, `quat.h`, `mat4.h`
auf, aber keine Hilfsfunktionen. `math_utils` enthaelt:
- Konstanten (PI, EPSILON, DEG2RAD)
- Skalar-Funktionen (clamp, lerp, smoothstep)
- Vec3-Hilfsfunktionen (distance, reflect, project)
- Mat4-Hilfsfunktionen (ortho, frustum)
- Utility (nextPowerOfTwo, isPowerOfTwo, nearEqual)

Diese Funktionen werden von allen Modulen intensiv genutzt.
Ohne zentrale Datei wuerde Code-Duplikation entstehen.

**Konsequenzen:** Keine – rein additive Aenderung.

---

### 7. Property-Tests mit rapidcheck statt handgeschrieben

**Roadmap:** "Property-based tests (rapidcheck)"  
**Tatsaechlich:** Vollstaendige rapidcheck-Integration in allen Modulen

**Begruendung:** Die Roadmap fordert rapidcheck, aber keine konkrete
Implementierung. Statt handgeschriebener Schleifen mit Zufallswerten
werden `rc::check()`-Makros verwendet, die:
- Automatisch Shrinking bei Fehlern durchfuehren
- Reproduzierbare Seeds fuer fehlgeschlagene Tests generieren
- Nahtlos in doctest integriert sind

**Konsequenzen:** Zusaetzliche Dependency (`rapidcheck` via vcpkg).
Build-Zeit steigt um ~5%.

---

### 8. Fuzz-Tests als Dual-Mode-Harnesses

**Roadmap:** "Fuzz tests (libFuzzer) for arena + serialize"  
**Tatsaechlich:** libFuzzer-Harnesses + doctest-Wrapper

**Begruendung:** libFuzzer-Tests koennen nicht direkt in CTest
laufen (erfordern Clang + `-fsanitize=fuzzer`). Die Dual-Mode-
Implementierung erlaubt:
- CI/CD: Tests laufen als normale Unit-Tests mit fixen Inputs
- Lokal: Entwickler koennen mit libFuzzer erweiterte Fuzzing-
  Kampagnen starten

**Konsequenzen:** Jede Fuzz-Datei hat `#ifdef SEED_FUZZ_AS_TEST`
Bloecke. Das ist Standard-Praxis in der Industrie.

---

### 9. `seed-core/vcpkg.json` als eigenes Manifest

**Roadmap:** Nur Root-`vcpkg.json`  
**Tatsaechlich:** Root-`vcpkg.json` + `seed-core/vcpkg.json`

**Begruendung:** Das Submodule sollte seine eigenen Dependencies
deklarieren (doctest, spdlog, fmt, nlohmann-json, rapidcheck).
Das ermoeglicht:
- Unabhaengige Nutzung von seed-core in anderen Projekten
- Klarere Dependency-Graphen
- Einfachere Version-Pinning pro Modul

**Konsequenzen:** Root-Manifest muss nicht alle Dependencies
wiederholen (Manifest-Inheritance via vcpkg).

---

## Nicht implementiert (bewusst zurueckgestellt)

| Deliverable | Grund | Plan |
|-------------|-------|------|
| `tests/unit/build/test_cmake.cpp` | Erstellt, aber Meta-Level | P0-Release |
| Property-Tests fuer Memory | rapidcheck-Integration komplex | P0-Release |
| Fuzz-Tests fuer Memory | libFuzzer erfordert Clang | P0-Release |
| Benchmarks fuer Math | Erstellt (`bench_math.cpp`) | P0-Release |

---

## Links

- [Roadmap: Phase 0 Fundament](../04_PHASE_0_FUNDAMENT.md)
- [Repo Architecture](../02_REPO_ARCHITECTURE.md)
- [API Documentation](../../submodules/seed-core/docs/API.md)
