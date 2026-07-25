#include "core/diagnostics/snapshot_dump.h"
#include "core/diagnostics/health_score.h"
#include "core/ecs/world.h"
#include <fmt/format.h>
#include <fstream>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <algorithm>

#if defined(_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif

#define SEED_STRINGIFY_IMPL(x) #x
#define SEED_STRINGIFY(x) SEED_STRINGIFY_IMPL(x)

#if defined(__clang__)
#  define SEED_COMPILER "clang " __clang_version__
#elif defined(__GNUC__)
#  define SEED_COMPILER "gcc " __VERSION__
#elif defined(_MSC_VER)
#  define SEED_COMPILER "msvc " SEED_STRINGIFY(_MSC_VER)
#else
#  define SEED_COMPILER "unknown"
#endif

#if defined(__linux__)
#  define SEED_PLATFORM "linux"
#elif defined(_WIN32)
#  define SEED_PLATFORM "windows"
#else
#  define SEED_PLATFORM "unknown"
#endif


// Assert hook for SEED_ASSERT integration
namespace seed {
    void (*g_seed_assert_hook)(const char* reason, const char* f, int l) = nullptr;
}

namespace seed::diagnostics {

namespace {
    // BUGFIX/NEU: vorher gab es keinen Weg, aus einem Signal-/SEH-Handler an
    // die aktive World zu kommen - SnapshotOnFailure::trigger() bekam bei
    // echten Crashes immer world=nullptr und lieferte damit nur Build-Info/
    // Health/Timeline statt eines vollstaendigen ECS-Snapshots. Mit
    // registerWorld() kann Anwendungscode (z. B. beim Start eines Levels/
    // Tests) die aktive World bekannt machen.
    const seed::ecs::World* g_worldForCrashDump = nullptr;

    const char* fatalSignalName(int sig) {
        switch (sig) {
            case SIGSEGV: return "SIGSEGV (ungueltiger Speicherzugriff)";
            case SIGABRT: return "SIGABRT (abort/assert)";
            case SIGFPE:  return "SIGFPE (Fliesskomma-/Ganzzahlfehler, z. B. Division durch 0)";
            case SIGILL:  return "SIGILL (ungueltige Instruktion)";
#if !defined(_WIN32)
            case SIGBUS:  return "SIGBUS (Bus-Fehler / Alignment)";
#endif
            default:      return "unbekanntes fatales Signal";
        }
    }

    // Gemeinsamer Handler-Koerper fuer alle registrierten POSIX-Signale.
    // WICHTIG: Handler darf bei einem echten Fault (SIGSEGV/SIGILL/SIGFPE/
    // SIGBUS) nicht normal zurueckkehren - die fehlerhafte Instruktion wuerde
    // sonst sofort erneut ausgefuehrt (Endlosschleife). Wir stellen den
    // Default-Handler wieder her und loesen das Signal danach erneut aus,
    // damit der Prozess mit demselben Signal/Exit-Code terminiert wie zuvor
    // (wichtig fuer CI/ctest, die genau das als Crash erkennen).
    void handleFatalSignal(int sig) {
        char reason[96];
        std::snprintf(reason, sizeof(reason), "Fatal signal %d (%s)", sig, fatalSignalName(sig));
        SnapshotOnFailure::trigger(reason, "<signal-handler>", 0, g_worldForCrashDump);

        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }

#if defined(_WIN32)
    LONG WINAPI handleWindowsException(EXCEPTION_POINTERS* info) {
        const DWORD code = info ? info->ExceptionRecord->ExceptionCode : 0;
        char reason[96];
        std::snprintf(reason, sizeof(reason), "Unhandled SEH exception 0x%08lX", static_cast<unsigned long>(code));
        SnapshotOnFailure::trigger(reason, "<seh-handler>", 0, g_worldForCrashDump);

        // Regulaere (unhandled) Weiterbehandlung durch das OS - der Prozess
        // terminiert danach genauso, wie er es ohne unseren Filter auch
        // getan haette; wir haengen uns nur fuer den Dump-Versuch ein.
        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif
} // namespace

void SnapshotDump::capture(const seed::ecs::World& world) {
    captureBuildInfo();
    captureHealth();
    captureTimeline();

    // BUGFIX: vorher wurde hier nur die Entity-Anzahl erfasst ("TODO: extend
    // World with toString()..."). Jetzt volle Archetype-/Entity-Uebersicht
    // ueber die public World::archetypeManager()-Introspektion.
    worldDump = "=== World Snapshot ===\n";
    worldDump += fmt::format("Entity count (alive): {}\n", world.entityCount());

    const auto& archMgr = world.archetypeManager();
    worldDump += fmt::format("Archetype count: {}\n", archMgr.archetypeCount());

    archetypeDump = "=== Archetypes ===\n";
    for (const auto& [id, archPtr] : archMgr) {
        const seed::ecs::Archetype& arch = *archPtr;

        archetypeDump += fmt::format("  [{:08x}] entities={} components=[",
                                      id.hash, arch.size());
        bool first = true;
        for (seed::ecs::ComponentType ct : arch.componentTypes()) {
            if (!first) archetypeDump += ", ";
            archetypeDump += fmt::format("{}", ct);
            first = false;
        }
        archetypeDump += "]\n";

        // Nur die ersten Entities pro Archetype auflisten, damit der Dump
        // bei grossen Welten nicht ausufert.
        const auto& ents = arch.entities();
        const size_t showCount = std::min<size_t>(ents.size(), 10);
        for (size_t i = 0; i < showCount; ++i) {
            archetypeDump += fmt::format("      entity idx={} version={}\n",
                                          seed::ecs::entityIndex(ents[i]),
                                          static_cast<unsigned>(seed::ecs::entityVersion(ents[i])));
        }
        if (ents.size() > showCount) {
            archetypeDump += fmt::format("      ... (+{} weitere)\n", ents.size() - showCount);
        }
    }
}

void SnapshotDump::captureTimeline() {
    eventTimeline = globalTimeline().dump();
}

void SnapshotDump::captureHealth() {
    healthReport = HealthScore::report();
}

void SnapshotDump::captureBuildInfo() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char timeStr[64] = {};
    struct tm timeInfo;
#if defined(_WIN32)
    localtime_s(&timeInfo, &time_t);
#else
    localtime_r(&time_t, &timeInfo);
#endif
    (void)std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);

    const char* diagStatus =
#if SEED_DIAGNOSTICS_ENABLED
        "enabled";
#else
        "disabled";
#endif

    buildInfo = std::string("Build Info:\n")
        .append("  Compiler: ").append(SEED_COMPILER).append("\n")
        .append("  Platform: ").append(SEED_PLATFORM).append("\n")
        .append("  Time:     ").append(timeStr).append("\n")
        .append("  Diagnostics: ").append(diagStatus).append("\n");
}

std::string SnapshotDump::toString() const {
    std::string out;
    out.reserve(4096);
    out += buildInfo;
    out += "\n";
    out += healthReport;
    out += "\n";
    out += worldDump;
    out += "\n";
    // BUGFIX: archetypeDump wurde bisher befuellt, aber nie ausgegeben.
    out += archetypeDump;
    out += "\n";
    out += "=== Event Timeline ===\n";
    out += eventTimeline;
    out += "\n";
    if (!errors.empty()) {
        out += "=== Errors ===\n";
        for (const auto& err : errors) {
            out += err;
            out += "\n";
        }
    }
    return out;
}

bool SnapshotDump::writeToFile(const std::string& path) const {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << toString();
    return ofs.good();
}

// ---------------------------------------------------------------------------
// SnapshotOnFailure
// ---------------------------------------------------------------------------

void SnapshotOnFailure::install() {
    // BUGFIX: vorher nur ein TODO-Kommentar, keine tatsaechliche Handler-
    // Registrierung - ein echter SIGSEGV/Access-Violation fuehrte zu keinem
    // Dump, nur zum sofortigen Prozessende durch das OS. Siehe Einschraenkungen
    // dazu im Header (snapshot_dump.h).
#if defined(_WIN32)
    SetUnhandledExceptionFilter(&handleWindowsException);
    std::signal(SIGABRT, &handleFatalSignal); // SEH faengt kein abort()/assert() ab
#else
    std::signal(SIGSEGV, &handleFatalSignal);
    std::signal(SIGABRT, &handleFatalSignal);
    std::signal(SIGFPE,  &handleFatalSignal);
    std::signal(SIGILL,  &handleFatalSignal);
    std::signal(SIGBUS,  &handleFatalSignal);
#endif

    // Register assert hook
    ::seed::g_seed_assert_hook = [](const char* reason, const char* f, int l) {
        trigger(reason, f, l, g_worldForCrashDump);
    };
}

void SnapshotOnFailure::registerWorld(const seed::ecs::World* world) noexcept {
    g_worldForCrashDump = world;
}

void SnapshotOnFailure::trigger(const char* reason,
                                const char* file,
                                int line,
                                const seed::ecs::World* world) {
    SnapshotDump dump;
    if (world) {
        dump.capture(*world);
    } else {
        dump.captureBuildInfo();
        dump.captureHealth();
        dump.captureTimeline();
    }
    dump.errors.push_back(fmt::format("{} at {}:{}", reason, file, line));
    writeEmergencyDump(dump);
}

void SnapshotOnFailure::writeEmergencyDump(const SnapshotDump& dump) {
    // BUGFIX: vorher immer derselbe Dateiname ("seed_crash_dump.txt") - ein
    // zweiter Crash im selben Arbeitsverzeichnis ueberschrieb den vorherigen
    // Dump ersatzlos. Jetzt ein Zeitstempel (ms seit Epoch) im Dateinamen,
    // damit mehrere Crash-Dumps nebeneinander erhalten bleiben.
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    char pathBuf[64];
    std::snprintf(pathBuf, sizeof(pathBuf), "seed_crash_dump_%lld.txt", static_cast<long long>(nowMs));
    const std::string path = pathBuf;

    if (dump.writeToFile(path)) {
        fmt::print(stderr, "\n!!! CRASH DUMP WRITTEN TO: {} !!!\n\n", path);
    } else {
        fmt::print(stderr, "\n!!! FAILED TO WRITE CRASH DUMP !!!\n");
        fmt::print(stderr, "{}", dump.toString());
    }
}

} // namespace seed::diagnostics
