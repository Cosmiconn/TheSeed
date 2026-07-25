# Fuzz Tests (libFuzzer)
# Note: libFuzzer requires clang and is not available in all environments.
# These tests serve as stubs and will be enabled in CI with clang.

find_package(doctest CONFIG REQUIRED)

add_executable(seed_fuzz_tests
    test_fuzz_arena.cpp
    test_fuzz_serialize.cpp
)

target_link_libraries(seed_fuzz_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_fuzz_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_fuzz_tests)
endif()

add_test(NAME seed_fuzz_tests COMMAND seed_fuzz_tests)
