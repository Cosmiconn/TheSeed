#include "FreelistAllocator.h"
#include <cassert>
#include <algorithm>

namespace memory {

FreelistAllocator::FreelistAllocator(size_t cap) : capacity(cap) {
    memory = std::make_unique<std::byte[]>(capacity);
    head = reinterpret_cast<Block*>(memory.get());
    head->size = capacity - sizeof(Block);
    head->next = nullptr;
    head->used = false;
}

FreelistAllocator::~FreelistAllocator() = default;

void* FreelistAllocator::Allocate(size_t size, size_t alignment) {
    Block* prev = nullptr;
    Block* curr = head;
    
    while (curr) {
        if (!curr->used) {
            uintptr_t blockStart = reinterpret_cast<uintptr_t>(curr) + sizeof(Block);
            uintptr_t aligned = (blockStart + alignment - 1) & ~(alignment - 1);
            size_t padding = aligned - blockStart;
            
            if (curr->size >= size + padding) {
                if (curr->size > size + padding + sizeof(Block) + 64) {
                    Block* newBlock = reinterpret_cast<Block*>(aligned + size);
                    newBlock->size = curr->size - size - padding - sizeof(Block);
                    newBlock->next = curr->next;
                    newBlock->used = false;
                    curr->next = newBlock;
                    curr->size = size + padding;
                }
                
                curr->used = true;
                return reinterpret_cast<void*>(aligned);
            }
        }
        prev = curr;
        curr = curr->next;
    }
    
    assert(false && "FreelistAllocator out of memory");
    return nullptr;
}

void FreelistAllocator::Free(void* ptr) {
    if (!ptr) return;
    
    Block* block = reinterpret_cast<Block*>(reinterpret_cast<std::byte*>(ptr) - sizeof(Block));
    block->used = false;
    
    // Coalesce
    Block* curr = head;
    while (curr && curr->next) {
        if (!curr->used && !curr->next->used) {
            curr->size += sizeof(Block) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void FreelistAllocator::Reset() {
    head = reinterpret_cast<Block*>(memory.get());
    head->size = capacity - sizeof(Block);
    head->next = nullptr;
    head->used = false;
}

} // namespace memory
