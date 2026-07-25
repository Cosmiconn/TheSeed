#pragma once

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// SEED_ASSERT – debug-only assertion
// ---------------------------------------------------------------------------
// Defined as a macro for file/line info, but uses an inline function
// for the actual implementation to avoid two-phase lookup issues
// in template member functions.
// ---------------------------------------------------------------------------

namespace seed {
    inline void seed_assert_impl(bool cond, const char* msg, const char* file, int line) {
#ifdef NDEBUG
        (void)cond; (void)msg; (void)file; (void)line;
#else
        if (!cond) {
            std::fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", msg, file, line);
            std::abort();
        }
#endif
    }
}

#define SEED_ASSERT(cond, msg) \
    ::seed::seed_assert_impl((cond), (msg), __FILE__, __LINE__)
