# =============================================================================
# seed-core – Core engine library
# =============================================================================
add_library(seed_core STATIC
    src/core/memory/block_allocator.cpp
    src/core/memory/arena_allocator.cpp
    src/core/memory/stack_allocator.cpp
    src/core/memory/memory_tracker.cpp
    src/core/memory/memory_system.cpp
    # ECS
    src/core/ecs/world.cpp
    src/core/ecs/archetype.cpp
    src/core/ecs/type_registry.cpp
    src/core/ecs/archetype_manager.cpp
    # Diagnostics
    src/core/diagnostics/event_timeline.cpp
    src/core/diagnostics/health_score.cpp
    src/core/diagnostics/ecs_validator.cpp
    src/core/diagnostics/memory_validator.cpp
    src/core/diagnostics/snapshot_dump.cpp
    src/core/diagnostics/diagnostics_manager.cpp
    # Job-System
    src/core/jobs/work_stealing_queue.cpp
    src/core/jobs/task.cpp
    src/core/jobs/task_graph.cpp
    src/core/jobs/job_system.cpp
    # Serialization
    src/core/serialize/binary_writer.cpp
    src/core/serialize/binary_reader.cpp
    src/core/serialize/snapshot.cpp
    src/core/serialize/delta.cpp
    src/core/serialize/reflection.cpp
    # Utilities
    src/core/util/random.cpp
    src/core/util/hash.cpp
    src/core/util/uuid.cpp
    src/core/util/config.cpp
)

target_include_directories(seed_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/public>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(seed_core PUBLIC
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
)

if(SEED_ENABLE_TRACY AND Tracy_FOUND)
  target_link_libraries(seed_core PUBLIC Tracy::TracyClient)
  target_compile_definitions(seed_core PUBLIC TRACY_ENABLE)
endif()

seed_apply_warnings(seed_core)

if(SEED_ENABLE_SANITIZERS)
  seed_apply_sanitizers(seed_core)
endif()

# =============================================================================
# Tests
# =============================================================================
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS seed_core
  EXPORT TheSeedTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY src/public/ DESTINATION include)
install(DIRECTORY src/core/ DESTINATION include/seed FILES_MATCHING PATTERN "*.h")
