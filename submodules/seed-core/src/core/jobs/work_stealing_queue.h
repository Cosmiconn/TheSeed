#pragma once

// ---------------------------------------------------------------------------
// WorkStealingQueue
// ---------------------------------------------------------------------------
// Lock-freie Chase-Lev Work-Stealing-Deque fuer Task*.
//
// Zugriffsregeln (WICHTIG, sonst Data Race):
//   - push()/pop(): NUR vom Owner-Thread (dem Worker, dem diese Queue gehoert)
//   - steal():      von JEDEM ANDEREN Thread
//
// Kapazitaet ist zur Laufzeit konfigurierbar (muss Zweierpotenz sein) und
// wird im Konstruktor des JobSystem aus JobSystemConfig::queueCapacity
// uebernommen.
// ---------------------------------------------------------------------------

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace seed::jobs {

struct Task;

class WorkStealingQueue {
public:
    explicit WorkStealingQueue(size_t capacity);
    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;
    WorkStealingQueue(WorkStealingQueue&&) = delete;
    WorkStealingQueue& operator=(WorkStealingQueue&&) = delete;

    // Nur vom Owner-Thread aufrufen. Gibt false zurueck, wenn die Deque voll
    // ist (Aufrufer muss dann selbst einen Fallback waehlen).
    bool push(Task* task) noexcept;

    // Nur vom Owner-Thread aufrufen.
    Task* pop() noexcept;

    // Von JEDEM Thread (auch dem Owner - aber der sollte pop() bevorzugen)
    // aufrufbar.
    Task* steal() noexcept;

    // Nur naeherungsweise korrekt bei gleichzeitigen push/pop/steal-Aufrufen -
    // ausschliesslich fuer Diagnose/Logging gedacht, nie fuer Kontrollfluss-
    // Entscheidungen verwenden.
    size_t approxSize() const noexcept;

private:
    std::unique_ptr<std::atomic<Task*>[]> m_buffer;
    size_t m_capacity;
    alignas(64) std::atomic<int64_t> m_top{0};
    alignas(64) std::atomic<int64_t> m_bottom{0};
};

} // namespace seed::jobs

#ifdef _MSC_VER
#pragma warning(pop)
#endif
