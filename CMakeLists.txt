cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Common settings
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compiler_warnings)
include(sanitizers)
include(test_macros)

# Options
option(SEED_BUILD_TESTS "Build tests" ON)
option(SEED_BUILD_BENCHMARKS "Build benchmarks" ON)
option(SEED_ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(SEED_ENABLE_TRACY "Enable Tracy profiling" OFF)

# Submodule builds
add_subdirectory(submodules/seed-core)

# Meta-Tests
if(SEED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
    add_subdirectory(tests/integration)
endif()
