#pragma once

// ---------------------------------------------------------------------------
// TheSeed Engine Diagnostics Framework (TEDF) – Configuration
// ---------------------------------------------------------------------------
// Based on: TheSeed_Engine_Diagnostics_Framework.md
// Phase: P0-M3 (ECS Integration)
//
// Usage:
//   #include "core/diagnostics/diagnostics_config.h"
//   #if SEED_DIAGNOSTICS_ENABLED
//     // diagnostic code
//   #endif
// ---------------------------------------------------------------------------

// ============================================================================
// Compile-Time Switches
// ============================================================================

#ifndef SEED_DIAGNOSTICS_ENABLED
#  if defined(NDEBUG)
#    define SEED_DIAGNOSTICS_ENABLED 0
#  else
#    define SEED_DIAGNOSTICS_ENABLED 1
#  endif
#endif

#ifndef SEED_DIAGNOSTICS_ECS_VALIDATION
#  define SEED_DIAGNOSTICS_ECS_VALIDATION SEED_DIAGNOSTICS_ENABLED
#endif

#ifndef SEED_DIAGNOSTICS_MEMORY_VALIDATION
#  define SEED_DIAGNOSTICS_MEMORY_VALIDATION SEED_DIAGNOSTICS_ENABLED
#endif

#ifndef SEED_DIAGNOSTICS_EVENT_TIMELINE
#  define SEED_DIAGNOSTICS_EVENT_TIMELINE SEED_DIAGNOSTICS_ENABLED
#endif

#ifndef SEED_DIAGNOSTICS_HEALTH_SCORE
#  define SEED_DIAGNOSTICS_HEALTH_SCORE SEED_DIAGNOSTICS_ENABLED
#endif

#ifndef SEED_DIAGNOSTICS_SNAPSHOT_ON_FAILURE
#  define SEED_DIAGNOSTICS_SNAPSHOT_ON_FAILURE SEED_DIAGNOSTICS_ENABLED
#endif

#ifndef SEED_DIAGNOSTICS_CONTRACTS
#  define SEED_DIAGNOSTICS_CONTRACTS SEED_DIAGNOSTICS_ENABLED
#endif

// ============================================================================
// Runtime Limits
// ============================================================================

#ifndef SEED_DIAGNOSTICS_MAX_EVENTS
#  define SEED_DIAGNOSTICS_MAX_EVENTS 10000
#endif

#ifndef SEED_DIAGNOSTICS_MAX_SNAPSHOT_SIZE
#  define SEED_DIAGNOSTICS_MAX_SNAPSHOT_SIZE (1024 * 1024) // 1MB
#endif

// ============================================================================
// Severity Levels (mirrors spdlog but independent)
// ============================================================================

namespace seed::diagnostics {

enum class Severity {
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Warning  = 3,
    Error    = 4,
    Critical = 5
};

inline const char* severityToString(Severity s) noexcept {
    switch (s) {
        case Severity::Trace:    return "TRACE";
        case Severity::Debug:    return "DEBUG";
        case Severity::Info:     return "INFO";
        case Severity::Warning:  return "WARN";
        case Severity::Error:    return "ERROR";
        case Severity::Critical: return "CRIT";
    }
    return "UNKNOWN";
}

} // namespace seed::diagnostics

// ============================================================================
// Contract Macros (Layer 1)
// ============================================================================

#if SEED_DIAGNOSTICS_CONTRACTS
#  define SEED_REQUIRES(cond, msg)      SEED_ASSERT((cond), "PRECONDITION FAILED: " msg)
#  define SEED_ENSURES(cond, msg)      SEED_ASSERT((cond), "POSTCONDITION FAILED: " msg)
#  define SEED_INVARIANT(cond, msg)      SEED_ASSERT((cond), "INVARIANT FAILED: " msg)
#else
#  define SEED_REQUIRES(cond, msg) ((void)0)
#  define SEED_ENSURES(cond, msg)  ((void)0)
#  define SEED_INVARIANT(cond, msg) ((void)0)
#endif
