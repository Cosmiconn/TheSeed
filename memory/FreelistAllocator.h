#pragma once
// =============================================================================
// memory/FreelistAllocator.h — Variable-size with free blocks list
// AP-14: Memory-Allocators
// =============================================================================
#include <cstdint>
#include <memory>

namespace memory {

class FreelistAllocator {
public:
    explicit FreelistAllocator(size_t capacity);
    ~FreelistAllocator();

    FreelistAllocator(const FreelistAllocator&) = delete;
    FreelistAllocator& operator=(const FreelistAllocator&) = delete;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = 64);
    void Free(void* ptr);
    void Reset();

private:
    struct Block {
        size_t size;
        Block* next;
        bool used;
    };

    size_t capacity = 0;
    std::unique_ptr<std::byte[]> memory;
    Block* head = nullptr;
};

} // namespace memory
