# Property-Based Tests (rapidcheck)
# ---------------------------------------------------------------------------
find_package(doctest CONFIG REQUIRED)
find_package(rapidcheck CONFIG REQUIRED)

add_executable(seed_meta_property_tests
    test_property_math.cpp
    test_property_random.cpp
    test_property_ecs.cpp
)

target_link_libraries(seed_meta_property_tests PRIVATE seed_core doctest::doctest rapidcheck)
seed_apply_warnings(seed_meta_property_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_meta_property_tests)
endif()

add_test(NAME seed_meta_property_tests COMMAND seed_meta_property_tests)
