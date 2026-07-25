# Job-System – TheSeed Engine

## Overview

Thread-pool with work-stealing queues (Chase-Lev algorithm). Supports task dependencies (DAG), barriers, and Tracy profiling integration. Zero deadlocks, zero false sharing.

## Architecture

```
[Main Thread] -> [Task A] -> [Task B]
[Worker 0] <-> [Local Queue] <-> [Steal from Worker 1]
[Worker N] <-> [Local Queue] <-> [Steal from Worker N-1]
```

## Key Components

| Component | File | Purpose |
|-----------|------|---------|
| `JobSystem` | `job_system.h/.cpp` | Thread pool orchestration |
| `WorkStealingQueue` | `work_stealing_queue.h/.cpp` | Lock-free per-worker queue |
| `Task` | `task.h/.cpp` | Task state machine |
| `TaskGraph` | `task_graph.h/.cpp` | DAG dependency management |

## API

```cpp
seed::jobs::JobSystem js;

// Submit simple task
js.submit([]() { /* work */ });

// Submit with dependencies
auto taskA = js.submit([]() { /* A */ });
auto taskB = js.submitAfter({taskA}, []() { /* B after A */ });

// Parallel for
js.parallelFor(10000, [&](size_t i) { data[i] = i; });

// Wait
js.waitForAll();
```

## Performance Targets

| Metric | Target | Test |
|--------|--------|------|
| Task throughput | 1M/sec | `gate_p0_jobs_throughput` |
| Work stealing | 8 workers, 8M tasks <2s | Benchmark |
| False sharing | None | `perf c2c` clean |
| Deadlocks | None | TSan clean |

## Testing

```bash
./scripts/test.sh JobSystem
```

## Deliverables

- `seed-core/src/core/jobs/job_system.h/.cpp`
- `seed-core/src/core/jobs/work_stealing_queue.h/.cpp`
- `seed-core/src/core/jobs/task.h/.cpp`
- `seed-core/src/core/jobs/task_graph.h/.cpp`
- Tests: unit, integration, benchmarks
