# Meta-level integration tests (cross-submodule)
# These tests verify that multiple submodules work together correctly.

find_package(doctest CONFIG REQUIRED)

add_executable(seed_integration_tests
    test_meta_build.cpp
)

target_link_libraries(seed_integration_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_integration_tests)
target_compile_definitions(seed_integration_tests PRIVATE SEED_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_integration_tests)
endif()

add_test(NAME seed_integration_tests COMMAND $<TARGET_FILE:seed_integration_tests>)
