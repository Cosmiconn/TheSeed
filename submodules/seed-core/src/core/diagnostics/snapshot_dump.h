#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/diagnostics/event_timeline.h"
#include <string>
#include <vector>

namespace seed::ecs {
    class World;
}

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// SnapshotDump – captures ECS state for crash reproduction (TEDF Layer 6)
// ---------------------------------------------------------------------------
// Captures:
//   - All archetypes with component signatures and entity counts
//   - All entity records (index, version, archetype, row)
//   - Event timeline (last N events)
//   - Health scores
//   - Build info (git commit, compiler, platform)
// ---------------------------------------------------------------------------
struct SnapshotDump {
    std::string buildInfo;
    std::string healthReport;
    std::string eventTimeline;
    std::string worldDump;
    std::string archetypeDump;
    std::vector<std::string> errors;

    void capture(const seed::ecs::World& world);
    void captureTimeline();
    void captureHealth();
    void captureBuildInfo();

    std::string toString() const;
    bool writeToFile(const std::string& path) const;
};

// ---------------------------------------------------------------------------
// Crash handler integration – auto-dump on assertion failure
// ---------------------------------------------------------------------------
// install() registriert:
//   - den SEED_ASSERT-Hook (wie zuvor)
//   - auf POSIX: sigaction-Handler fuer SIGSEGV/SIGABRT/SIGFPE/SIGILL/SIGBUS
//   - auf Windows: SetUnhandledExceptionFilter (Access Violation etc.) plus
//     signal(SIGABRT, ...) (SEH faengt kein abort()/assert() ab)
//
// WICHTIG - bewusste Einschraenkung: Der Handler ruft intern std::string/
// fmt::format auf (ueber SnapshotDump), was streng genommen nicht
// async-signal-sicher ist. Das ist ein pragmatischer, in Spiele-Engines
// gaengiger Kompromiss (Dump-Versuch schlaegt im Zweifel fehl, statt gar
// keine Diagnose zu liefern) - kein hartes Echtzeit-/Signal-Safety-Garantie.
// Eine voll gehaertete Variante (vorallozierte Puffer, kein Heap im Handler,
// sigaltstack fuer Stack-Overflow) ist bewusst nicht Teil dieser Aenderung
// und gehoert eher zu M6 (Profiler/Logger/Crash-Handler) im Projekt-Fahrplan.
//
// Der Handler terminiert den Prozess nach dem Dump-Versuch immer mit dem
// urspruenglichen Signal/derselben Exception (kein "Weiterlaufen nach Crash"),
// damit CI/ctest den Fehler weiterhin korrekt als Crash erkennen.
// ---------------------------------------------------------------------------
class SnapshotOnFailure {
public:
    static void install();
    static void trigger(const char* reason,
                        const char* file,
                        int line,
                        const seed::ecs::World* world = nullptr);

    // Registriert die aktive World, damit ein Crash-Handler (Signal/SEH oder
    // SEED_ASSERT) einen vollstaendigen ECS-Snapshot ziehen kann statt nur
    // Build-Info/Health/Timeline. nullptr deregistriert wieder.
    static void registerWorld(const seed::ecs::World* world) noexcept;

private:
    static void writeEmergencyDump(const SnapshotDump& dump);
};

#if SEED_DIAGNOSTICS_SNAPSHOT_ON_FAILURE
#  define SEED_SNAPSHOT_ON_FAIL(world, reason)      ::seed::diagnostics::SnapshotOnFailure::trigger((reason), __FILE__, __LINE__, &(world))
#else
#  define SEED_SNAPSHOT_ON_FAIL(world, reason) ((void)0)
#endif

} // namespace seed::diagnostics
