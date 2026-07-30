# Fuzz Tests (libFuzzer-compatible harnesses)
# ---------------------------------------------------------------------------
# Build with: clang++ -fsanitize=fuzzer,address ...
# Note: These are harnesses that can be compiled with libFuzzer.
# For CI, they run as simplified unit tests with fixed inputs.
# ---------------------------------------------------------------------------

add_executable(seed_meta_fuzz_arena
    test_fuzz_arena.cpp
)
target_link_libraries(seed_meta_fuzz_arena PRIVATE seed_core)
seed_apply_warnings(seed_meta_fuzz_arena)
target_compile_definitions(seed_meta_fuzz_arena PRIVATE SEED_FUZZ_AS_TEST)

add_executable(seed_meta_fuzz_serialize
    test_fuzz_serialize.cpp
)
target_link_libraries(seed_meta_fuzz_serialize PRIVATE seed_core)
seed_apply_warnings(seed_meta_fuzz_serialize)
target_compile_definitions(seed_meta_fuzz_serialize PRIVATE SEED_FUZZ_AS_TEST)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_meta_fuzz_arena)
    seed_apply_sanitizers(seed_meta_fuzz_serialize)
endif()

# CTest registration (runs with fixed seed input)
add_test(NAME seed_meta_fuzz_arena_smoke COMMAND $<TARGET_FILE:seed_meta_fuzz_arena>)
add_test(NAME seed_meta_fuzz_serialize_smoke COMMAND $<TARGET_FILE:seed_meta_fuzz_serialize>)
