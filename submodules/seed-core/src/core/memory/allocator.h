#pragma once

#include <cstddef>
#include <cstdint>

namespace seed::memory {

// ---------------------------------------------------------------------------
// Abstract allocator interface
// ---------------------------------------------------------------------------
class Allocator {
public:
    virtual ~Allocator() = default;

    virtual void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
    virtual void  deallocate(void* ptr, size_t size = 0) = 0;
    virtual size_t getAllocationSize(const void* ptr) const { (void)ptr; return 0; }
};

// ---------------------------------------------------------------------------
// Utility: align pointer up
// ---------------------------------------------------------------------------
inline uintptr_t align_up(uintptr_t value, size_t alignment) {
    const size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

inline void* align_ptr(void* ptr, size_t alignment) {
    return reinterpret_cast<void*>(align_up(reinterpret_cast<uintptr_t>(ptr), alignment));
}

} // namespace seed::memory
