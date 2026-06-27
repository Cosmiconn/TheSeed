#include "StackAllocator.h"
#include <cassert>

namespace memory {

StackAllocator::StackAllocator(size_t cap) : capacity(cap), offset(0) {
    memory = std::make_unique<std::byte[]>(capacity);
}

StackAllocator::~StackAllocator() = default;

void* StackAllocator::Allocate(size_t size, size_t alignment) {
    uintptr_t current = reinterpret_cast<uintptr_t>(memory.get()) + offset;
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;
    
    assert(offset + padding + size <= capacity && "StackAllocator out of memory");
    
    offset += padding + size;
    return reinterpret_cast<void*>(aligned);
}

void StackAllocator::FreeToMarker(size_t marker) {
    assert(marker <= offset && "Invalid marker");
    offset = marker;
}

void StackAllocator::Reset() {
    offset = 0;
}

} // namespace memory
