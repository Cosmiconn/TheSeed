#include "core/jobs/task.h"

// Task selbst hat keine out-of-line Methoden (alles im Header inline definiert).
// Diese Datei existiert als eigenes Translation Unit fuer statische
// Sanity-Checks, damit ein Verstoss gegen die erwarteten Eigenschaften von
// Task schon beim Bauen von seed_core auffaellt und nicht erst beim ersten
// Verwender (job_system.cpp / task_graph.cpp).

namespace seed::jobs {
namespace {

static_assert(!std::is_copy_constructible_v<Task>,
              "Task darf nicht kopierbar sein - wird ausschliesslich per "
              "Task* (Pool-Allokation) referenziert.");
static_assert(sizeof(Task) >= sizeof(void*),
              "Task muss mindestens pointergross sein (Voraussetzung fuer "
              "PoolAllocator<Task>, siehe pool_allocator.h).");

} // namespace
} // namespace seed::jobs
