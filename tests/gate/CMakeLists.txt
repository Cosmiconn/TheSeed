# Gate Tests – Funktional (blockieren CI)
add_executable(seed_gate_tests
    test_gate_p0.cpp
    test_gate_p0_diagnostics.cpp
)
target_link_libraries(seed_gate_tests PRIVATE seed_core)
seed_apply_warnings(seed_gate_tests)
target_compile_definitions(seed_gate_tests PRIVATE SEED_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME seed_gate_tests COMMAND seed_gate_tests)

# Gate Tests – Performance (informational, blockieren CI NICHT)
add_executable(seed_gate_perf_tests
    test_gate_p0_perf.cpp
)
target_link_libraries(seed_gate_perf_tests PRIVATE seed_core)
seed_apply_warnings(seed_gate_perf_tests)
add_test(NAME seed_gate_perf_tests COMMAND seed_gate_perf_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_gate_tests)
    seed_apply_sanitizers(seed_gate_perf_tests)
endif()
