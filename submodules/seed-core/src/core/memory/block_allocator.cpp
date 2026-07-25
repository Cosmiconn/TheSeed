#include "core/memory/block_allocator.h"
#include "core/diagnostics/event_timeline.h"
#include "core/diagnostics/diagnostics_config.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Platform-specific OS allocation
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace seed::memory {

// ---------------------------------------------------------------------------
// OS allocation helpers
// ---------------------------------------------------------------------------
void* BlockAllocator::os_alloc(size_t size, size_t alignment) {
    SEED_ZONE("BlockAllocator::os_alloc");

#ifdef _WIN32
    // VirtualAlloc always returns allocation-granularity-aligned (64 KiB) memory.
    (void)alignment;
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) {
        return nullptr;
    }
    return ptr;
#else
    // mmap only guarantees page alignment. To honour larger alignments we
    // over-allocate and trim head/tail so the returned mapping spans exactly
    // the requested (page-rounded) size at the requested alignment.
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    const size_t page = (pageSize > 0) ? static_cast<size_t>(pageSize) : 4096;
    const size_t align = (alignment > page) ? alignment : page;

    const size_t rounded = (size + page - 1) & ~(page - 1);
    const size_t total   = rounded + align - page;

    uint8_t* raw = static_cast<uint8_t*>(
        mmap(nullptr, total, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (raw == MAP_FAILED) {
        return nullptr;
    }

    uint8_t* aligned = reinterpret_cast<uint8_t*>(
        align_up(reinterpret_cast<uintptr_t>(raw), align));

    const size_t head = static_cast<size_t>(aligned - raw);
    const size_t tail = total - head - rounded;
    if (head > 0) {
        munmap(raw, head);
    }
    if (tail > 0) {
        munmap(aligned + rounded, tail);
    }
    return aligned;
#endif
}

void BlockAllocator::os_free(void* ptr, size_t size) {
    SEED_ZONE("BlockAllocator::os_free");

#ifdef _WIN32
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    // os_alloc trims the mapping to the page-rounded size, so munmap must use
    // the same rounding to release the whole mapping.
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    const size_t page = (pageSize > 0) ? static_cast<size_t>(pageSize) : 4096;
    const size_t rounded = (size + page - 1) & ~(page - 1);
    munmap(ptr, rounded);
#endif
}

// ---------------------------------------------------------------------------
// BlockAllocator implementation
// ---------------------------------------------------------------------------
BlockAllocator::BlockAllocator(size_t blockSize, size_t alignment)
    : m_blockSize(blockSize), m_alignment(alignment) {}

BlockAllocator::~BlockAllocator() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& block : m_blocks) {
        if (block.base) {
            os_free(block.base, block.size);
            SEED_FREE(block.base);
        }
    }
    m_blocks.clear();
}

void* BlockAllocator::allocate(size_t size, size_t alignment) {
    SEED_ZONE("BlockAllocator::allocate");
    SEED_DIAG_EVENT(seed::diagnostics::EventType::MemoryAllocate, seed::ecs::INVALID_ENTITY,
                    0, 0, static_cast<uint32_t>(size),
                    "BlockAllocator::allocate", __FILE__, __LINE__);

    if (size > m_blockSize) {
        return nullptr;
    }

    size_t allocSize = m_blockSize;
    size_t align     = (alignment > m_alignment) ? alignment : m_alignment;

    void* ptr = os_alloc(allocSize, align);
    if (!ptr) {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_blocks.push_back({ptr, allocSize, true});
    }

    m_totalAllocated.fetch_add(allocSize, std::memory_order_relaxed);
    m_totalUsed.fetch_add(allocSize, std::memory_order_relaxed);

    SEED_ALLOC(ptr, allocSize);
    return ptr;
}

void BlockAllocator::deallocate(void* ptr, size_t size) {
    SEED_ZONE("BlockAllocator::deallocate");

    if (!ptr) return;

    size_t freedSize = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& block : m_blocks) {
            if (block.base == ptr) {
                freedSize = block.size;
                block.inUse = false;
                os_free(block.base, block.size);
                SEED_FREE(block.base);
                block.base = nullptr; // Mark as freed
                break;
            }
        }
    }

    if (freedSize == 0) {
        return;
    }

    m_totalUsed.fetch_sub(freedSize, std::memory_order_relaxed);

    if (size > 0 && size != freedSize) {
        // Size mismatch – debug builds could assert here
        (void)0;
    }
}

size_t BlockAllocator::getAllocationSize(const void* ptr) const {
    if (!ptr) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& block : m_blocks) {
        if (block.base == ptr) {
            return block.size;
        }
    }
    return 0;
}

size_t BlockAllocator::numBlocks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.size();
}

std::vector<BlockAllocator::BlockInfo> BlockAllocator::blockList() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BlockInfo> list;
    list.reserve(m_blocks.size());
    for (const auto& b : m_blocks) {
        list.push_back({b.base, b.size, b.inUse});
    }
    return list;
}

} // namespace seed::memory
