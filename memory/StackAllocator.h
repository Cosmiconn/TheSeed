#pragma once
// =============================================================================
// memory/StackAllocator.h — Linear/Ring stack allocator
// AP-14: Memory-Allocators
// =============================================================================
#include <cstdint>
#include <memory>
#include <vector>

namespace memory {

class StackAllocator {
public:
    explicit StackAllocator(size_t capacity);
    ~StackAllocator();

    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = 64);
    void FreeToMarker(size_t marker);
    void Reset();

    [[nodiscard]] size_t GetMarker() const { return offset; }
    [[nodiscard]] size_t GetCapacity() const { return capacity; }
    [[nodiscard]] size_t GetUsed() const { return offset; }

private:
    size_t capacity = 0;
    size_t offset = 0;
    std::unique_ptr<std::byte[]> memory;
};

} // namespace memory
