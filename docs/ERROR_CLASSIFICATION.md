# Fehlerklassifizierung – TheSeed ECS & Diagnostics

## Kategorie A: Kritisch (Prozess-Abbruch erzwungen)

| Code | Beschreibung | Erkennung | Maßnahme |
|------|-------------|-----------|----------|
| A1 | Entity-Version-Mismatch (Zombie-Handle) | `isAlive()` | Assert + Snapshot |
| A2 | Entity-Record ↔ Archetype-Index Inkonsistenz | `validateInvariants()` | Assert + Snapshot |
| A3 | ComponentType-ID-Kollision | `TypeRegistry::registerComponent()` | Assert bei Registrierung |
| A4 | Column-Größen-Mismatch | `EcsValidator::validateArchetype()` | Assert + Snapshot |
| A5 | Speicher-Korruption (nullptr Column) | `EcsValidator::validateComponentMemory()` | Assert + Snapshot |

## Kategorie B: Hoch (Log + Health-Score-Degradation)

| Code | Beschreibung | Erkennung | Maßnahme |
|------|-------------|-----------|----------|
| B1 | Memory-Budget überschritten | `MemoryTracker::checkBudget()` | Alarm-Callback + Log |
| B2 | Frame-Budget überschritten | `FrameTimer::isOverBudget()` | Warn-Log |
| B3 | Archetype-Hash-Kollision (Signature divergiert) | `ArchetypeId::operator==` | Keine – wird durch vollständigen Signaturvergleich abgefangen |

## Kategorie C: Mittel (Warn-Log, keine Degradation)

| Code | Beschreibung | Erkennung | Maßnahme |
|------|-------------|-----------|----------|
| C1 | Duplicate `addComponent` (Wert-Update) | `World::addComponent()` | Silent update, Debug-Log optional |
| C2 | `removeComponent` auf nicht vorhandenem Typ | `World::removeComponent()` | Safe no-op |
| C3 | Operation auf totem Handle | `World::*` | Safe no-op / nullptr |

## Kategorie D: Informational (Timeline-Eintrag)

| Code | Beschreibung | Erkennung |
|------|-------------|-----------|
| D1 | Entity erstellt | Timeline: `EntityCreate` |
| D2 | Entity zerstört | Timeline: `EntityDestroy` |
| D3 | Archetype-Migration | Timeline: `ComponentMove` |
| D4 | Komponente hinzugefügt | Timeline: `ComponentAdd` |

## Recovery-Matrix

| Fehler | Automatisch behandelbar? | Benötigt Restart? | Benötigt Code-Fix? |
|--------|------------------------|-------------------|-------------------|
| A1–A5 | Nein | Ja | Ja |
| B1 | Ja (Budget anpassen) | Nein | Optional |
| B2 | Ja (System-Optimierung) | Nein | Optional |
| C1–C3 | Ja (defensives API-Verhalten) | Nein | Nein |
