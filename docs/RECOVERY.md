# Recovery Strategy – TheSeed Diagnostics Framework

## Ziel
Bei einem Invariant-Failure oder Crash maximale Diagnose-Information erhalten und den Fehler reproduzieren.

## Automatische Maßnahmen bei Fehlern

### 1. ECS-Invariant-Failure (Debug/Diagnostics Builds)
- `SEED_VALIDATE_WORLD(world)` erkennt die Verletzung.
- **Vor** dem `abort()` wird automatisch ein `SnapshotDump` ausgelöst.
- Ausgabe: `seed_crash_dump_<timestamp>.txt` mit:
  - Build-Info (Compiler, Platform, Git-Commit)
  - Health-Score aller Module
  - Letzte 10.000 Events aus der Timeline
  - Vollständiger ECS-World-Dump (Archetypen, Entities, Komponenten)

### 2. Signal-/Exception-Handler (Release Builds)
- POSIX: `SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGBUS`
- Windows: `SetUnhandledExceptionFilter` + `SIGABRT`
- Handler-Vorgehen:
  1. Snapshot der registrierten `World` ziehen (falls `registerWorld()` aufgerufen).
  2. Emergency-Dump auf Platte schreiben (zeitstempelbasiert, keine Überschreibung).
  3. Ursprüngliches Signal/Exception erneut auslösen → Prozess terminiert mit korrektem Exit-Code.

### 3. Assert-Integration
- `SEED_ASSERT(cond, msg)` ruft den globalen `g_seed_assert_hook` auf.
- Hook löst `SnapshotOnFailure::trigger()` aus.
- Danach `std::abort()` für sofortigen Prozessstopp.

## Manuelle Maßnahmen

### Während der Entwicklung
- Nach jedem signifikanten ECS-Operation-Block `CHECK_INVARIANTS(world)` aufrufen.
- Bei Performance-Problemen: Tracy-Profiler starten (`TRACY_ENABLE`).

### In der CI
- Jeder Build läuft mit ASan + UBSan + LSan.
- Stresstests (10× MultiThreadStress, 10× 100kEntities) müssen grün sein.
- Statische Analyse (`clang-tidy`, `cppcheck`) darf keine neuen Warnings einführen.

## Grenzen & Kompromisse

- **Signal-Safety**: Der Crash-Handler verwendet `std::string`/`fmt::format`. Dies ist streng genommen nicht async-signal-safe, aber der pragmatische Standard in Game-Engines. Eine vollständig signal-sichere Variante (pre-allozierte Buffer, kein Heap) ist für M6 geplant.
- **World-Registrierung**: Ohne `SnapshotOnFailure::registerWorld(&world)` enthält der Dump nur Build-Info und Timeline. Die Anwendung muss die aktive World beim Level-Start registrieren.
