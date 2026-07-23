# Test Macros - Unified test registration
# Usage: seed_add_test(target_name source1 [source2 ...])

function(seed_add_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE doctest::doctest)
    target_enable_warnings(${target})
    target_enable_sanitizers(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()
