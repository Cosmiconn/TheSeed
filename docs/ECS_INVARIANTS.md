# ECS Invariants – TheSeed Engine

> Dokumentation der harten Garantien, die das ECS-System zu jedem Zeitpunkt einhält.

## Entity-System Invariants

1. **Einheitlicher Index-Version-Handle**
   - Ein `Entity`-Handle kodiert einen 24-Bit-Index und einen 8-Bit-Version.
   - `isAlive(e)` prüft **stets** beides: Index muss innerhalb `m_entities` liegen UND Version muss übereinstimmen.

2. **Recycling-Sicherheit**
   - Beim Wiederverwenden eines freien Slots wird die Version inkrementiert (`+1`, Wrap-around bei `0→1`).
   - Ein alter Handle wird dadurch **dauerhaft** ungültig – auch nach 255 Recycles desselben Slots.

3. **Entity ↔ Record Konsistenz**
   - `m_records[index]` enthält für jede lebende Entity den aktuellen `ArchetypeId` und den Row-Index.
   - `validateInvariants()` prüft: `archetype.entityAt(record.index) == entity`.

4. **Entity ↔ Archetype Konsistenz**
   - `Archetype::removeEntityByIndex(index)` führt stets **Swap-and-Pop** durch.
   - Die verschobene Entity aktualisiert ihren Record-Index in `World::destroyEntity()` und `World::moveEntity()`.

## Archetype Invariants

5. **Sortierte Signatur**
   - `ArchetypeId.signature` ist immer aufsteigend sortiert.
   - `makeArchetypeId()` garantiert deterministische Hash-Bildung über die sortierte Signatur.

6. **Kollisionsfreiheit**
   - `ArchetypeId::operator==` vergleicht **Hash + vollständige Signatur**.
   - Hash-Kollisionen verschiedener Signaturen führen nicht zur falschen Archetype-Wiederverwendung.

7. **Column-Größen-Konsistenz**
   - Alle `IComponentArray`-Spalten eines Archetypes haben dieselbe `size()`.
   - `Archetype::addEntity()` erweitert alle Spalten synchron.
   - `Archetype::removeEntityByIndex()` reduziert alle Spalten synchron.

## Component-Lifetime Invariants

8. **Triviale & nicht-triviale Destruktoren**
   - `ComponentArray::remove()` ruft stets `meta().destruct()` auf dem entfernten Element auf.
   - `ComponentArray::move()` ruft `destruct(dst)` → `move(dst, src)` → `destruct(src)` auf.
   - Move-only-Typen (z. B. `std::unique_ptr`) werden korrekt transferiert, nicht kopiert.

9. **Duplicate-Add-Verhalten**
   - `World::addComponent<T>(e, ...)` auf einer Entity, die `T` bereits besitzt, führt zu einem **Wert-Update** (Zuweisung), nicht zu einer zweiten Komponente.
   - Es gibt pro Entity maximal **eine** Instanz eines Component-Typs.

10. **Safe No-Ops auf ungültigen Handles**
    - `getComponent<T>(deadHandle)` → `nullptr`
    - `hasComponent<T>(deadHandle)` → `false`
    - `removeComponent<T>(deadHandle)` → no-op
    - `destroyEntity(deadHandle)` → no-op
