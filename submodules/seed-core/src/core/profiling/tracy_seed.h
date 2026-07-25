#pragma once

#include "core/profiling/seed_assert.h"

// ---------------------------------------------------------------------------
// Tracy integration – SINGLE source of truth for all profiling macros.
// Include this header everywhere; never redefine SEED_ZONE / SEED_ALLOC etc.
// ---------------------------------------------------------------------------

#if __has_include(<tracy/Tracy.hpp>)
#  include <tracy/Tracy.hpp>
#  define SEED_ZONE(name)       ZoneScopedN(name)
#  define SEED_ALLOC(ptr, sz)   TracyAlloc(ptr, sz)
#  define SEED_FREE(ptr)        TracyFree(ptr)
#  define SEED_FRAME_MARK()     FrameMark
#else
#  define SEED_ZONE(name)       ((void)0)
#  define SEED_ALLOC(ptr, sz)   ((void)0)
#  define SEED_FREE(ptr)        ((void)0)
#  define SEED_FRAME_MARK()     ((void)0)
#endif
