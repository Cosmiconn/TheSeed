#pragma once

#include "core/memory/allocator.h"
#include "core/profiling/tracy_seed.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace seed::memory {

// ---------------------------------------------------------------------------
// BlockAllocator
// ---------------------------------------------------------------------------
// Allocates large aligned blocks directly from the OS.
// Thread-safe.  Intended as the root allocator for Pool-, Arena-, etc.
//
// Windows: VirtualAlloc
// Linux:   mmap (anonymous, MAP_PRIVATE)
// ---------------------------------------------------------------------------
class BlockAllocator : public Allocator {
public:
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024 * 1024; // 64 MiB
    static constexpr size_t DEFAULT_ALIGNMENT  = 64 * 1024;       // 64 KiB page alignment

    explicit BlockAllocator(size_t blockSize = DEFAULT_BLOCK_SIZE,
                            size_t alignment = DEFAULT_ALIGNMENT);
    ~BlockAllocator() override;

    // Disable copy / move
    BlockAllocator(const BlockAllocator&) = delete;
    BlockAllocator& operator=(const BlockAllocator&) = delete;
    BlockAllocator(BlockAllocator&&) = delete;
    BlockAllocator& operator=(BlockAllocator&&) = delete;

    // Allocator interface
    void* allocate(size_t size, size_t alignment = DEFAULT_ALIGNMENT) override;
    void  deallocate(void* ptr, size_t size = 0) override;
    size_t getAllocationSize(const void* ptr) const override;

    // Stats
    size_t totalAllocated() const { return m_totalAllocated.load(std::memory_order_relaxed); }
    size_t totalUsed()      const { return m_totalUsed.load(std::memory_order_relaxed); }
    size_t numBlocks()      const;

    // For tests / debugging
    struct BlockInfo {
        void*  base;
        size_t size;
        bool   inUse;
    };
    std::vector<BlockInfo> blockList() const;

private:
    struct Block {
        void*  base;
        size_t size;
        bool   inUse;
    };

    size_t m_blockSize;
    size_t m_alignment;

    mutable std::mutex m_mutex;
    std::vector<Block> m_blocks;

    std::atomic<size_t> m_totalAllocated{0};
    std::atomic<size_t> m_totalUsed{0};

    // OS abstraction
    static void* os_alloc(size_t size, size_t alignment);
    static void  os_free(void* ptr, size_t size);
};

} // namespace seed::memory
