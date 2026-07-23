# Sanitizers - ASan, UBSan, TSan
# Usage: target_enable_sanitizers(target_name)

function(target_enable_sanitizers target)
    if(NOT SEED_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()
