#include "core/jobs/task_graph.h"
#include "core/jobs/job_system.h"

namespace seed::jobs {

TaskHandle TaskGraph::createTask(Task::Func work, const char* name) {
    TaskHandle handle = m_jobSystem.createTask(std::move(work), name);
    m_tasks.push_back(handle);
    return handle;
}

void TaskGraph::addDependency(TaskHandle before, TaskHandle after) {
    m_jobSystem.addDependency(before, after);
}

void TaskGraph::submitAll() {
    for (TaskHandle t : m_tasks) {
        m_jobSystem.submit(t);
    }
}

} // namespace seed::jobs
