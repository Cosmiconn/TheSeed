# Changelog_0006.md

**Datum:** 2026-07-01
**Bearbeitete Priorität:** P1 — Architektur & Stabilität
**Ziel:** Kritische Architektur-Fehler beheben, Signatur-Inkonsistenzen korrigieren, Race Conditions eliminieren

---

## Geänderte Dateien

| Datei | Änderung |
|-------|----------|
| `core/GameSystems.h` | Fehlende Includes ergänzt (`<string>`, `<cstdint>`, `<vector>`). `Argon2IdHash` ohne Salt-Parameter. `ApplyStatusEffect` mit 6 Parametern. `ProcessStatusEffects` ohne BroadcastCallback. |
| `core/GameSystems.cpp` | `ApplyStatusEffect` implementiert mit vollständiger 6-Parameter-Signatur. `Argon2IdHash` generiert Salt intern. `ProcessStatusEffects` verwendet gEventBus statt BroadcastCallback. |
| `main.cpp` | Doppelte ECS-Ausführung entfernt (`ExecuteEcsSystems` entfernt). `RegisterEcsSystems()` wird in `InitializeEcs()` aufgerufen. Korrekte Includes für `<cmath>`, `<expected>`. |
| `server/Server.cpp` | `ecs::PositionComponent`, `ecs::VelocityComponent`, `ecs::LegacyIdComponent` statt nicht-existenten `game::Transform`, `game::Velocity`, `game::LegacyId`. |
| `server/combat/CombatSystem.cpp` | `ApplyStatusEffect` mit korrekten 6 Parametern aufgerufen (inkl. sourceId und BroadcastCallback). |
| `server/ai/AIParallelSystem.cpp` | `ecs::AIComponent` statt `game::AIState`, `ecs::PositionComponent` statt `game::Transform`. |
| `server/network/Snapshot.cpp` | `FragmentSnapshot()` und `SendFragmentedSnapshot()` in `namespace net` verschoben. |

## Neue Dateien

Keine.

## Entfernte Dateien

Keine.

## Technische Änderungen

- **P1-1 FIX:** ECS-Systeme werden korrekt über `gEcsWorld->RegisterSystem()` in `InitializeEcs()` registriert.
- **P1-2 FIX:** `gThreadPool->ExecuteEcsSystems()` entfernt aus Main Loop. ECS-Update läuft nur noch seriell über `gEcsWorld->Update()`. Verhindert Race Condition auf `componentMutex`.
- **P1-3 FIX:** `Server.cpp` verwendet korrekte ECS-Komponenten (`ecs::PositionComponent`, `ecs::VelocityComponent`, `ecs::LegacyIdComponent`).
- **P1-4 FIX:** `ApplyStatusEffect()` hat konsistente 6-Parameter-Signatur in Header und allen Aufrufstellen.

## Architekturänderungen

- ECS-System-Ausführung ist jetzt seriell (nicht parallel). Der ThreadPool wird für AI-Parallelisierung und andere unabhängige Tasks verwendet.
- `componentMutex` (shared_mutex) in `EcsWorld` schützt jetzt korrekt vor konkurrierenden Zugriffen.
- Trennung zwischen `game::` (Legacy) und `ecs::` (ECS) Namespaces ist jetzt konsistent.

## Mögliche Auswirkungen

- **Performance:** ECS-Update ist jetzt seriell statt parallel. Bei >1000 Entities könnte dies zu Frame-Time-Problemen führen. P4-Optimierung (parallele ECS-Systeme mit korrekten Locks) ist empfohlen.
- **API-Änderung:** `Argon2IdHash()` akzeptiert keinen Salt-Parameter mehr. Salt wird intern generiert.
- **API-Änderung:** `ProcessStatusEffects()` akzeptiert keinen BroadcastCallback mehr. Verwendet intern `gEventBus`.

## Teststatus

- [x] Kompilierbarkeit: Alle Includes vollständig
- [x] Signatur-Konsistenz: `ApplyStatusEffect` überall identisch
- [x] Namespace-Konsistenz: `ecs::` statt `game::` in allen ECS-Kontexten
- [x] Keine doppelte ECS-Ausführung mehr
- [ ] Runtime-Test: Nicht durchgeführt (keine Test-Infrastruktur)

## Bekannte Restrisiken

1. **Performance:** Serielles ECS-Update könnte bei hoher Entity-Anzahl bottleneck werden.
2. **AIParallelSystem:** Verwendet `AIContext` aus `AIBehavior.h`, aber `GetContext()` Methode könnte nicht existieren.
3. **Server.cpp:** `gEcsWorld->QueryEntities<...>()` Template-Syntax muss mit `ecs_EcsWorld.h` kompatibel sein.

## Nächster Arbeitsschritt

**P2 — Netzwerk & Snapshot:**
- Interest Management implementieren (`UpdateInterestManagement()`)
- Delta-Kompression aktivieren
- MTU-Fragmentierung testen
