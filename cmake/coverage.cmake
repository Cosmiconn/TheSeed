# coverage.cmake – Code coverage instrumentation (gcov + lcov)
# Usage: cmake -DSEED_ENABLE_COVERAGE=ON ...

option(SEED_ENABLE_COVERAGE "Enable code coverage (gcov + lcov)" OFF)

function(seed_apply_coverage target)
  if(NOT SEED_ENABLE_COVERAGE)
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(WARNING "Coverage only supported for GCC/Clang: ${CMAKE_CXX_COMPILER_ID}")
    return()
  endif()

  target_compile_options(${target} PUBLIC --coverage -O0 -g)
  # Atomic profile update prevents negative hit counts in multithreaded tests
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${target} PUBLIC -fprofile-update=atomic)
  endif()
  target_link_options(${target} PUBLIC --coverage)
  message(STATUS "Coverage enabled: ${target}")
endfunction()

function(seed_generate_coverage_report)
  if(NOT SEED_ENABLE_COVERAGE)
    return()
  endif()

  find_program(LCOV_PROGRAM lcov)
  find_program(GENHTML_PROGRAM genhtml)

  if(NOT LCOV_PROGRAM OR NOT GENHTML_PROGRAM)
    message(WARNING "lcov/genhtml not found – coverage report generation skipped")
    return()
  endif()

  set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
  set(COVERAGE_INFO "${COVERAGE_DIR}/coverage.info")

  add_custom_target(coverage-report
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
    # Zero counters
    COMMAND ${LCOV_PROGRAM} --directory ${CMAKE_BINARY_DIR} --zerocounters
    # Run tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -E seed_gate_perf_tests
    # Capture coverage data
    COMMAND ${LCOV_PROGRAM} --directory ${CMAKE_BINARY_DIR} --capture --output-file ${COVERAGE_INFO} --ignore-errors negative
    # Remove external / system headers
    COMMAND ${LCOV_PROGRAM} --remove ${COVERAGE_INFO}
      '/usr/*'
      '${CMAKE_BINARY_DIR}/*'
      '${CMAKE_SOURCE_DIR}/submodules/seed-core/tests/*'
      '${CMAKE_SOURCE_DIR}/tests/*'
      '${CMAKE_SOURCE_DIR}/vcpkg/*'
      --output-file ${COVERAGE_INFO}.filtered
      --ignore-errors negative
    # Generate HTML report
    COMMAND ${GENHTML_PROGRAM} ${COVERAGE_INFO}.filtered
      --output-directory ${COVERAGE_DIR}/html
      --title "TheSeed Coverage Report"
      --legend
    COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: ${COVERAGE_DIR}/html/index.html"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report..."
    VERBATIM
  )
endfunction()
