cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.")
endif()

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

# Common settings
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compiler_warnings)
include(sanitizers)
include(test_macros)
include(coverage)

# ccache acceleration (CI + local builds)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
endif()

# Options
option(SEED_BUILD_TESTS "Build tests" ON)
option(SEED_ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(SEED_ENABLE_TRACY "Enable Tracy profiling" OFF)

# Dependencies
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)

if(SEED_ENABLE_TRACY)
  find_package(Tracy CONFIG)
  if(Tracy_FOUND)
    message(STATUS "Tracy found - profiling instrumentation enabled")
  else()
    message(WARNING "SEED_ENABLE_TRACY=ON but Tracy package not found - continuing without Tracy")
  endif()
endif()

# Testing
if(SEED_BUILD_TESTS)
  enable_testing()
endif()

# Submodule-Builds
add_subdirectory(submodules/seed-core)
add_subdirectory(submodules/seed-network)

# Meta-level tests
if(SEED_BUILD_TESTS)
  add_subdirectory(tests/integration)
  add_subdirectory(tests/e2e)
  add_subdirectory(tests/gate)
  add_subdirectory(tests/property)
  add_subdirectory(tests/fuzz)
endif()

# Coverage report target
seed_generate_coverage_report()

# Install
install(EXPORT TheSeedTargets
  FILE TheSeedTargets.cmake
  NAMESPACE TheSeed::
  DESTINATION lib/cmake/TheSeed
)
