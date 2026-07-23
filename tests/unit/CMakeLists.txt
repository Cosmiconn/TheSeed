# Meta-level unit tests

include(${CMAKE_SOURCE_DIR}/cmake/test_macros.cmake)

seed_add_test(test_meta_build
    build/test_cmake.cpp
)
