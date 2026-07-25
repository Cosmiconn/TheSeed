#include "core/diagnostics/memory_validator.h"
#include "core/memory/block_allocator.h"
#include "core/memory/memory_tracker.h"
#include "core/memory/memory_system.h"
#include <fmt/format.h>

namespace seed::diagnostics {

using namespace seed::memory;

bool MemoryValidator::validateAll(MemoryValidationResult* outResult) {
    MemoryValidationResult localResult;
    MemoryValidationResult& result = outResult ? *outResult : localResult;

    bool ok = true;

    // Validate global block allocator if initialized
    if (g_blockAllocator) {
        ok &= validateBlockAllocator(*g_blockAllocator, &result);
    }

    // Validate memory tracker if initialized
    if (g_memoryTracker) {
        ok &= validateMemoryTracker(*g_memoryTracker, &result);
    }

    // Check for leaks
    ok &= checkLeaks(&result);

    return ok;
}

bool MemoryValidator::validateBlockAllocator(const BlockAllocator& alloc,
                                             MemoryValidationResult* /*outResult*/) {
    (void)alloc;
    // TODO: extend BlockAllocator with validation hooks
    return true;
}

bool MemoryValidator::validateMemoryTracker(const MemoryTracker& tracker,
                                           MemoryValidationResult* /*outResult*/) {
    (void)tracker;
    // TODO: extend MemoryTracker with category iteration
    return true;
}

bool MemoryValidator::checkLeaks(MemoryValidationResult* /*outResult*/) {
    // TODO: integrate with Tracy or custom allocation tracking
    // For now, rely on ASan/LSan in CI builds
    return true;
}

std::string MemoryValidator::fullReport() {
    std::string out;
    out += "=== Memory Validation Report ===\n";
    if (g_blockAllocator) {
        out += "  BlockAllocator: initialized\n";
    } else {
        out += "  BlockAllocator: NOT initialized\n";
    }
    if (g_memoryTracker) {
        out += "  MemoryTracker: initialized\n";
    } else {
        out += "  MemoryTracker: NOT initialized\n";
    }
    out += "================================\n";
    return out;
}

bool MemoryValidator::checkAlignment(const void* ptr, size_t alignment,
                                     MemoryValidationResult& result) {
    const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    if ((addr & (alignment - 1)) != 0) {
        result.fail("Pointer alignment mismatch", __FILE__, __LINE__);
        return false;
    }
    return true;
}

bool MemoryValidator::checkPointerValidity(const void* ptr,
                                           MemoryValidationResult& result) {
    if (!ptr) {
        result.fail("Null pointer where valid pointer expected", __FILE__, __LINE__);
        return false;
    }
    return true;
}

} // namespace seed::diagnostics
