# submodule_helpers.cmake – Shared helpers for TheSeed meta-repo submodule management
include_guard(GLOBAL)

# seed_add_submodule – safely add a submodule directory if it exists
function(seed_add_submodule name)
    set(path "submodules/${name}")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${path}/CMakeLists.txt")
        message(STATUS "Adding submodule: ${name}")
        add_subdirectory("${path}")
    else()
        message(WARNING "Submodule ${name} not found at ${path} – skipping")
    endif()
endfunction()

# seed_check_submodules – verify all expected submodules are initialized
function(seed_check_submodules)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path = ([^\n]+)" matches "${gitmodules}")
    foreach(match IN LISTS matches)
        string(REGEX REPLACE "path = " "" submod_path "${match}")
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submod_path}/.git")
            message(FATAL_ERROR "Submodule ${submod_path} not initialized. Run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()
