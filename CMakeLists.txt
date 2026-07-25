cmake_minimum_required(VERSION 3.25)
project(TheSeed VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compiler_warnings)
include(sanitizers)
include(test_macros)

# =============================================================================
# Dependencies (vcpkg)
# =============================================================================
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)

option(SEED_ENABLE_TRACY "Enable Tracy profiling instrumentation" OFF)
if(SEED_ENABLE_TRACY)
  find_package(Tracy CONFIG)
  if(Tracy_FOUND)
    message(STATUS "Tracy found – profiling instrumentation enabled")
  else()
    message(WARNING "SEED_ENABLE_TRACY=ON but Tracy package not found – continuing without Tracy")
  endif()
endif()

# =============================================================================
# Testing
# =============================================================================
if(BUILD_TESTING)
  enable_testing()
endif()

# =============================================================================
# Submodules
# =============================================================================
add_subdirectory(submodules/seed-core)

# =============================================================================
# Meta-level tests (cross-submodule integration + e2e + gate + property + fuzz)
# =============================================================================
if(BUILD_TESTING)
  add_subdirectory(tests/integration)
  add_subdirectory(tests/e2e)
  add_subdirectory(tests/gate)
  add_subdirectory(tests/property)
  add_subdirectory(tests/fuzz)
endif()

# =============================================================================
# Install
# =============================================================================
install(EXPORT TheSeedTargets
  FILE TheSeedTargets.cmake
  NAMESPACE TheSeed::
  DESTINATION lib/cmake/TheSeed
)
