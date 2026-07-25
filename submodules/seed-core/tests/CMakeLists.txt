find_package(doctest CONFIG REQUIRED)

add_executable(seed_tests
    unit/main.cpp
    unit/memory/test_block_allocator.cpp
    unit/memory/test_pool_allocator.cpp
    unit/memory/test_arena_allocator.cpp
    unit/memory/test_stack_allocator.cpp
    unit/memory/test_memory_tracker.cpp
    unit/memory/test_integration.cpp
    # ECS tests
    unit/ecs/test_ecs_basics.cpp
    unit/ecs/test_ecs_performance.cpp
    # Diagnostics tests
    unit/diagnostics/test_diagnostics.cpp
    # Job-System tests (Month 4)
    unit/jobs/test_job_system.cpp
    # Serialization tests (Month 5)
    unit/serialize/test_binary.cpp
    unit/serialize/test_snapshot.cpp
    unit/serialize/test_delta.cpp
    unit/serialize/test_reflection.cpp
    # Math tests (Month 6)
    unit/math/test_vec3.cpp
    unit/math/test_quat.cpp
    unit/math/test_mat4.cpp
    # Utility tests (Month 6)
    unit/util/test_random.cpp
    unit/util/test_hash.cpp
    unit/util/test_uuid.cpp
    unit/util/test_config.cpp
)

target_link_libraries(seed_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_tests)
endif()

# Register tests with CTest (doctest_discover_tests requires doctest cmake module)
add_test(NAME seed_unit_tests COMMAND seed_tests)

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
add_executable(seed_bench_block
    benchmarks/bench_block_allocator.cpp
)
target_link_libraries(seed_bench_block PRIVATE seed_core)
seed_apply_warnings(seed_bench_block)

add_executable(seed_bench_pool
    benchmarks/bench_pool_allocator.cpp
)
target_link_libraries(seed_bench_pool PRIVATE seed_core)
seed_apply_warnings(seed_bench_pool)

add_executable(seed_bench_arena
    benchmarks/bench_arena_allocator.cpp
)
target_link_libraries(seed_bench_arena PRIVATE seed_core)
seed_apply_warnings(seed_bench_arena)

add_executable(seed_bench_integration
    benchmarks/bench_integration.cpp
)
target_link_libraries(seed_bench_integration PRIVATE seed_core)
seed_apply_warnings(seed_bench_integration)

# ECS benchmark
add_executable(seed_bench_ecs
    benchmarks/bench_ecs.cpp
)
target_link_libraries(seed_bench_ecs PRIVATE seed_core)
seed_apply_warnings(seed_bench_ecs)

# Job-System benchmark
add_executable(seed_bench_jobs
    benchmarks/bench_jobs.cpp
)
target_link_libraries(seed_bench_jobs PRIVATE seed_core)
seed_apply_warnings(seed_bench_jobs)

# Serialization benchmark (Month 5)
add_executable(seed_bench_serialize
    benchmarks/bench_serialize.cpp
)
target_link_libraries(seed_bench_serialize PRIVATE seed_core)
seed_apply_warnings(seed_bench_serialize)
