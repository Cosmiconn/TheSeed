option(SEED_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

function(seed_apply_sanitizers target)
  if(NOT SEED_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined,leak
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined,leak
    )
    message(STATUS "Sanitizers enabled (GCC/Clang: ASan/UBSan/LSan): ${target}")

  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC ASan is tricky with vcpkg dependencies and /INCREMENTAL.
    # For now, skip MSVC ASan in CI. Use /RTC1 for debug runtime checks instead.
    target_compile_options(${target} PRIVATE /RTC1)
    message(STATUS "Runtime checks enabled (MSVC /RTC1): ${target}")
  else()
    message(WARNING "Sanitizers not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()
