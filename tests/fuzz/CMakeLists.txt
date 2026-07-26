# Fuzz Tests (libFuzzer-compatible harnesses)
# ---------------------------------------------------------------------------
# Build with: clang++ -fsanitize=fuzzer,address ...
# Note: These are harnesses that can be compiled with libFuzzer.
# For CI, they run as simplified unit tests with fixed inputs.
# ---------------------------------------------------------------------------

add_executable(seed_fuzz_arena
    test_fuzz_arena.cpp
)
target_link_libraries(seed_fuzz_arena PRIVATE seed_core)
seed_apply_warnings(seed_fuzz_arena)

add_executable(seed_fuzz_serialize
    test_fuzz_serialize.cpp
)
target_link_libraries(seed_fuzz_serialize PRIVATE seed_core)
seed_apply_warnings(seed_fuzz_serialize)

# CTest registration (runs with fixed seed input)
add_test(NAME seed_fuzz_arena_smoke COMMAND seed_fuzz_arena)
add_test(NAME seed_fuzz_serialize_smoke COMMAND seed_fuzz_serialize)
