# Property-Based Tests (rapidcheck)
# Note: rapidcheck is listed as a dependency but not yet integrated.
# These tests serve as stubs and will be enabled once rapidcheck is available.

find_package(doctest CONFIG REQUIRED)

add_executable(seed_property_tests
    test_property_math.cpp
    test_property_random.cpp
    test_property_ecs.cpp
)

target_link_libraries(seed_property_tests PRIVATE seed_core doctest::doctest)
seed_apply_warnings(seed_property_tests)

if(SEED_ENABLE_SANITIZERS)
    seed_apply_sanitizers(seed_property_tests)
endif()

add_test(NAME seed_property_tests COMMAND seed_property_tests)
