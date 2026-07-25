#pragma once

#include "core/diagnostics/diagnostics_config.h"
#include "core/memory/allocator.h"
#include "core/memory/memory_tracker.h"
#include <string>
#include <vector>

namespace seed::memory {
    class BlockAllocator;
    class PoolAllocatorBase;
    class ArenaAllocator;
    class StackAllocator;
}

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// MemoryValidationResult – result of memory validation pass
// ---------------------------------------------------------------------------
struct MemoryValidationResult {
    bool        success = true;
    std::string message;
    const char* file = "";
    int         line = 0;
    size_t      leakedBytes = 0;
    size_t      leakedAllocations = 0;
    size_t      corruptedBlocks = 0;

    void fail(const char* msg, const char* f = "", int l = 0,
              size_t leaked = 0, size_t allocs = 0, size_t corrupt = 0) noexcept {
        success = false;
        message = msg;
        file = f;
        line = l;
        leakedBytes = leaked;
        leakedAllocations = allocs;
        corruptedBlocks = corrupt;
    }
};

// ---------------------------------------------------------------------------
// MemoryValidator – validates all allocator subsystems (TEDF Layer 2)
// ---------------------------------------------------------------------------
// Checks:
//   - BlockAllocator: alignment, no double-free, no use-after-free
//   - PoolAllocator: free-list consistency, no dangling pointers
//   - ArenaAllocator: no overflow, reset correctness
//   - StackAllocator: LIFO order, marker consistency
//   - MemoryTracker: budget compliance, alarm accuracy
// ---------------------------------------------------------------------------
class MemoryValidator {
public:
    // Validate all global allocators
    static bool validateAll(MemoryValidationResult* outResult = nullptr);

    // Individual allocator validators
    static bool validateBlockAllocator(const seed::memory::BlockAllocator& alloc,
                                       MemoryValidationResult* outResult = nullptr);
    static bool validateMemoryTracker(const seed::memory::MemoryTracker& tracker,
                                      MemoryValidationResult* outResult = nullptr);

    // Check for leaks (requires allocator to track active allocations)
    static bool checkLeaks(MemoryValidationResult* outResult = nullptr);

    // Full report
    static std::string fullReport();

private:
    static bool checkAlignment(const void* ptr, size_t alignment,
                               MemoryValidationResult& result);
    static bool checkPointerValidity(const void* ptr,
                                     MemoryValidationResult& result);
};

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------
#if SEED_DIAGNOSTICS_MEMORY_VALIDATION
#  define SEED_VALIDATE_MEMORY()      do {          ::seed::diagnostics::MemoryValidationResult _vr;          if (!::seed::diagnostics::MemoryValidator::validateAll(&_vr)) {              SEED_ASSERT(false, _vr.message.c_str());          }      } while(0)
#else
#  define SEED_VALIDATE_MEMORY() ((void)0)
#endif

} // namespace seed::diagnostics
