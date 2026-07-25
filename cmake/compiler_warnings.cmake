function(seed_apply_warnings target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Werror -Wshadow -Wnon-virtual-dtor
      -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
      -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion
      -Wformat=2 -Wimplicit-fallthrough
      $<$<CXX_COMPILER_ID:GNU>:-Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast>
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /w14640 /w14826 /w14928)
  endif()
endfunction()
