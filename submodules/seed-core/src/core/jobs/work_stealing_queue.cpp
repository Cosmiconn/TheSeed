#include "core/jobs/work_stealing_queue.h"
#include <cassert>

namespace seed::jobs {

WorkStealingQueue::WorkStealingQueue(size_t capacity)
    : m_buffer(std::make_unique<std::atomic<Task*>[]>(capacity))
    , m_capacity(capacity) {
    assert((capacity & (capacity - 1)) == 0 &&
           "WorkStealingQueue capacity must be a power of two");
    // std::atomic<T*> default-constructs to nullptr (C++20 guarantee)
}

bool WorkStealingQueue::push(Task* task) noexcept {
    const int64_t b = m_bottom.load(std::memory_order_relaxed);
    const int64_t t = m_top.load(std::memory_order_acquire);
    if (b - t >= static_cast<int64_t>(m_capacity)) {
        return false; // voll
    }
    m_buffer[static_cast<size_t>(b) & (m_capacity - 1)]
        .store(task, std::memory_order_relaxed);
    m_bottom.store(b + 1, std::memory_order_release);
    return true;
}

Task* WorkStealingQueue::pop() noexcept {
    int64_t b = m_bottom.load(std::memory_order_relaxed) - 1;
    m_bottom.store(b, std::memory_order_seq_cst);
    int64_t t = m_top.load(std::memory_order_seq_cst);

    if (t > b) {
        m_bottom.store(b + 1, std::memory_order_relaxed);
        return nullptr;
    }

    Task* task = m_buffer[static_cast<size_t>(b) & (m_capacity - 1)]
                     .load(std::memory_order_relaxed);

    if (t == b) {
        if (!m_top.compare_exchange_strong(t, t + 1,
                                            std::memory_order_seq_cst,
                                            std::memory_order_relaxed)) {
            task = nullptr;
        }
        m_bottom.store(b + 1, std::memory_order_relaxed);
    }
    return task;
}

Task* WorkStealingQueue::steal() noexcept {
    int64_t t = m_top.load(std::memory_order_seq_cst);
    int64_t b = m_bottom.load(std::memory_order_seq_cst);

    if (t >= b) {
        return nullptr; // leer
    }

    Task* task = m_buffer[static_cast<size_t>(t) & (m_capacity - 1)]
                     .load(std::memory_order_relaxed);

    if (!m_top.compare_exchange_strong(t, t + 1,
                                        std::memory_order_seq_cst,
                                        std::memory_order_relaxed)) {
        return nullptr;
    }
    return task;
}

size_t WorkStealingQueue::approxSize() const noexcept {
    const int64_t b = m_bottom.load(std::memory_order_relaxed);
    const int64_t t = m_top.load(std::memory_order_relaxed);
    return (b > t) ? static_cast<size_t>(b - t) : 0;
}

} // namespace seed::jobs
