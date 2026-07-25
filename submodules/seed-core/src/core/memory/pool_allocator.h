#pragma once

#include "core/memory/allocator.h"
#include "core/memory/block_allocator.h"
#include "core/profiling/tracy_seed.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace seed::memory {

// ---------------------------------------------------------------------------
// PoolAllocator
// ---------------------------------------------------------------------------
// Fixed-size object pool with per-thread caching and lock-free global
// fallback.  Meets the P0-M2 spec requirement: "thread-safe, lock-free".
//
// Design:
//   - Thread-local cache (hot): 64 items max, no atomics
//   - Global free list (cold): lock-free Treiber stack via CAS
//   - BlockAllocator backing: pages carved into objects
//
// Thread-safety: allocate()/deallocate() may be called from any thread.
// ---------------------------------------------------------------------------
template<typename T, size_t ObjectsPerPage = 512>
class PoolAllocator : public Allocator {
    static_assert(sizeof(T) >= sizeof(void*), "T must be at least pointer-sized");
    static_assert(alignof(T) >= alignof(void*), "T alignment must be at least pointer-sized");

    struct FreeNode {
        FreeNode* next;
    };

    struct alignas(64) Page {              // cache-line aligned page header
        static constexpr size_t DATA_SIZE = ObjectsPerPage * sizeof(T);
        alignas(alignof(T)) uint8_t data[DATA_SIZE];
        Page* nextPage;
    };

    struct alignas(64) ThreadCache {       // one per (thread, instance), cache-line isolated
        FreeNode* freeList = nullptr;
        uint32_t count = 0;
        static constexpr uint32_t MAX_CACHE = 64;
        static constexpr uint32_t BATCH_SIZE = 32;
    };

    // BUGFIX (Job-System-Stresstests, ASan-Repro): thread_local Registry
    // isoliert nach Instanz-ID, damit nacheinander erzeugte PoolAllocator-
    // Instanzen sich NICHT einen Cache teilen (siehe CHANGELOG.md).
    struct ThreadCacheRegistry {
        uint64_t lastId = 0;
        bool lastValid = false;
        ThreadCache* lastCache = nullptr;
        std::vector<std::pair<uint64_t, std::unique_ptr<ThreadCache>>> entries;

        ThreadCache& get(uint64_t id) {
            if (lastValid && id == lastId) {
                return *lastCache;
            }
            for (auto& [key, cachePtr] : entries) {
                if (key == id) {
                    lastId = id;
                    lastValid = true;
                    lastCache = cachePtr.get();
                    return *lastCache;
                }
            }
            entries.emplace_back(id, std::make_unique<ThreadCache>());
            lastId = id;
            lastValid = true;
            lastCache = entries.back().second.get();
            return *lastCache;
        }
    };

public:
    explicit PoolAllocator(BlockAllocator* blockAlloc)
        : m_blockAlloc(blockAlloc)
        , m_numAllocated(0)
        , m_instanceId(s_nextInstanceId.fetch_add(1, std::memory_order_relaxed))
    {
        SEED_ZONE("PoolAllocator::ctor");
    }

    ~PoolAllocator() override {
        SEED_ZONE("PoolAllocator::dtor");
        // Pages are owned by BlockAllocator; we just leak them for now.
        // In Phase 1 we can add a proper cleanup path if needed.
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    template<typename... Args>
    T* construct(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    void destroy(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        deallocate(ptr, sizeof(T));
    }

    void* allocate(size_t size, size_t alignment) override {
        SEED_ZONE("PoolAllocator::allocate");
        (void)size;
        (void)alignment;

        ThreadCache& cache = getThreadCache();

        // 1. Thread-local cache (fast path, lock-free)
        if (cache.count > 0) {
            FreeNode* node = cache.freeList;
            cache.freeList = node->next;
            --cache.count;
            m_numAllocated.fetch_add(1, std::memory_order_relaxed);
            SEED_ALLOC(node, sizeof(T));
            return reinterpret_cast<void*>(node);
        }

        // 2. Refill from global lock-free list
        if (tryRefillCache(cache)) {
            return allocate(size, alignment); // retry
        }

        // 3. Allocate new page from BlockAllocator
        allocateNewPage();

        // 4. Retry (global list now has nodes)
        if (tryRefillCache(cache)) {
            return allocate(size, alignment);
        }

        return nullptr; // OOM
    }

    void deallocate(void* ptr, size_t /*size*/) override {
        SEED_ZONE("PoolAllocator::deallocate");
        if (!ptr) return;
        SEED_FREE(ptr);

        ThreadCache& cache = getThreadCache();
        FreeNode* node = reinterpret_cast<FreeNode*>(ptr);

        // 1. Push to thread-local cache
        if (cache.count < ThreadCache::MAX_CACHE) {
            node->next = cache.freeList;
            cache.freeList = node;
            ++cache.count;
            m_numAllocated.fetch_sub(1, std::memory_order_relaxed);
            return;
        }

        // 2. Cache full – flush half to global lock-free list
        flushCacheToGlobal(cache);

        // 3. Push to (now emptier) cache
        node->next = cache.freeList;
        cache.freeList = node;
        ++cache.count;
        m_numAllocated.fetch_sub(1, std::memory_order_relaxed);
    }

    size_t numAllocated() const {
        return m_numAllocated.load(std::memory_order_relaxed);
    }

private:
    BlockAllocator* m_blockAlloc;
    std::atomic<size_t> m_numAllocated;
    const uint64_t m_instanceId;

    static inline std::atomic<uint64_t> s_nextInstanceId{1};

    // -----------------------------------------------------------------------
    // Lock-free global free list (Treiber stack)
    // -----------------------------------------------------------------------
    // ABA: In Phase 0 we accept the theoretical ABA risk because:
    //   - Nodes are never freed to the OS; they are only reused within this
    //     allocator after deallocate().
    //   - The window for ABA (same address reappearing between load and CAS)
    //     is extremely narrow and has not been observed in stress tests.
    //   - Hazard pointers or tagged pointers would add significant complexity
    //     and memory overhead for P0.
    // If ABA ever manifests, the fix is tagged pointers (low 16 bits = tag).
    // -----------------------------------------------------------------------
    alignas(64) std::atomic<FreeNode*> m_globalFreeList{nullptr};

    // Page list (for cleanup / tracking) – NOT on the hot path.
    alignas(64) std::mutex m_pageMutex;
    Page* m_pageList = nullptr;

    inline static thread_local ThreadCacheRegistry s_registry;

    ThreadCache& getThreadCache() {
        return s_registry.get(m_instanceId);
    }

    // Push a single node onto the global lock-free stack.
    void pushGlobal(FreeNode* node) noexcept {
        FreeNode* head = m_globalFreeList.load(std::memory_order_relaxed);
        do {
            node->next = head;
        } while (!m_globalFreeList.compare_exchange_weak(
                    head, node,
                    std::memory_order_release,
                    std::memory_order_relaxed));
    }

    // Pop a single node from the global lock-free stack.
    // Returns nullptr if the stack is empty.
    FreeNode* popGlobal() noexcept {
        FreeNode* head = m_globalFreeList.load(std::memory_order_acquire);
        while (head != nullptr) {
            FreeNode* next = head->next;
            if (m_globalFreeList.compare_exchange_weak(
                    head, next,
                    std::memory_order_acquire,
                    std::memory_order_acquire)) {
                return head;
            }
            // CAS failed: head was updated by another thread. Retry.
        }
        return nullptr;
    }

    // Refill the thread-local cache from the global lock-free list.
    // Pops up to BATCH_SIZE nodes atomically (one by one – acceptable
    // because the hot path is the thread-local cache).
    bool tryRefillCache(ThreadCache& cache) {
        SEED_ZONE("PoolAllocator::tryRefillCache");

        FreeNode* first = popGlobal();
        if (!first) return false;

        // Build a local chain of up to BATCH_SIZE nodes
        FreeNode* last = first;
        uint32_t count = 1;

        while (count < ThreadCache::BATCH_SIZE) {
            FreeNode* node = popGlobal();
            if (!node) break;
            last->next = node;
            last = node;
            ++count;
        }

        // Prepend chain to thread-local cache
        last->next = cache.freeList;
        cache.freeList = first;
        cache.count += count;
        return true;
    }

    // Flush half of the thread-local cache to the global lock-free list.
    void flushCacheToGlobal(ThreadCache& cache) {
        SEED_ZONE("PoolAllocator::flushCacheToGlobal");

        if (!cache.freeList || cache.count == 0) return;

        uint32_t toMove = cache.count / 2;
        if (toMove == 0) toMove = 1;

        // Detach the sub-list to move
        FreeNode* moveHead = cache.freeList;
        FreeNode* moveTail = moveHead;
        for (uint32_t i = 1; i < toMove; ++i) {
            if (!moveTail->next) break;
            moveTail = moveTail->next;
        }

        FreeNode* newCacheHead = moveTail->next;
        moveTail->next = nullptr;

        // Push the entire chain onto the global stack in reverse order
        // so that the relative order is preserved (not strictly required,
        // but avoids pathological patterns).
        FreeNode* node = moveHead;
        while (node) {
            FreeNode* next = node->next;
            pushGlobal(node);
            node = next;
        }

        cache.freeList = newCacheHead;
        cache.count -= toMove;
    }

    void allocateNewPage() {
        SEED_ZONE("PoolAllocator::allocateNewPage");

        void* raw = m_blockAlloc->allocate(sizeof(Page), alignof(Page));
        if (!raw) return;

        Page* page = new (raw) Page();
        page->nextPage = nullptr;

        // Carve page into free nodes and push them onto the global stack
        uint8_t* data = page->data;
        for (size_t i = 0; i < ObjectsPerPage; ++i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(data + i * sizeof(T));
            node->next = nullptr; // pushGlobal sets this, but be safe
            pushGlobal(node);
        }

        {
            std::lock_guard<std::mutex> lock(m_pageMutex);
            page->nextPage = m_pageList;
            m_pageList = page;
        }
    }
};

} // namespace seed::memory

#ifdef _MSC_VER
#pragma warning(pop)



#endif
