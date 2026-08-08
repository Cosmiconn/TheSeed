# End-to-end tests (full engine simulation)
# These tests simulate real game scenarios.

find_package(doctest CONFIG REQUIRED)

add_executable(seed_e2e_tests
    test_smoke.cpp
    test_e2e_diagnostics.cpp
)

target_link_libraries(seed_e2e_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_e2e_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_e2e_tests)
endif()

add_test(NAME seed_e2e_tests COMMAND $<TARGET_FILE:seed_e2e_tests>)
