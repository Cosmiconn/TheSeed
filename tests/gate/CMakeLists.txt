# Gate tests for Phase 0
find_package(doctest CONFIG REQUIRED)

add_executable(seed_gate_tests
    test_gate_p0.cpp
)

target_link_libraries(seed_gate_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_gate_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_gate_tests)
endif()

add_test(NAME seed_gate_tests COMMAND $<TARGET_FILE:seed_gate_tests>)
