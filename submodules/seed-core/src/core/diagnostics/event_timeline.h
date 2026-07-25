#pragma once

#if defined(_MSC_VER)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

#include "core/diagnostics/diagnostics_config.h"
#include "core/ecs/entity.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace seed::diagnostics {

// ---------------------------------------------------------------------------
// EventType – all ECS and Memory operations that can be logged
// ---------------------------------------------------------------------------
enum class EventType : uint8_t {
    EntityCreate,
    EntityDestroy,
    EntityRecycle,
    ComponentAdd,
    ComponentRemove,
    ComponentMove,
    ComponentDestruct,
    ComponentDefaultConstruct,
    ArchetypeCreate,
    ArchetypeDestroy,
    MemoryAllocate,
    MemoryDeallocate,
    SystemUpdate,
    WorldValidate,
    InvariantFail,
    AssertionFail,
    Custom
};

inline const char* eventTypeToString(EventType t) noexcept {
    switch (t) {
        case EventType::EntityCreate:           return "EntityCreate";
        case EventType::EntityDestroy:          return "EntityDestroy";
        case EventType::EntityRecycle:        return "EntityRecycle";
        case EventType::ComponentAdd:           return "ComponentAdd";
        case EventType::ComponentRemove:        return "ComponentRemove";
        case EventType::ComponentMove:          return "ComponentMove";
        case EventType::ComponentDestruct:      return "ComponentDestruct";
        case EventType::ComponentDefaultConstruct: return "ComponentDefaultConstruct";
        case EventType::ArchetypeCreate:      return "ArchetypeCreate";
        case EventType::ArchetypeDestroy:     return "ArchetypeDestroy";
        case EventType::MemoryAllocate:         return "MemoryAllocate";
        case EventType::MemoryDeallocate:     return "MemoryDeallocate";
        case EventType::SystemUpdate:         return "SystemUpdate";
        case EventType::WorldValidate:        return "WorldValidate";
        case EventType::InvariantFail:        return "InvariantFail";
        case EventType::AssertionFail:        return "AssertionFail";
        case EventType::Custom:               return "Custom";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// DiagnosticEvent – single entry in the timeline
// ---------------------------------------------------------------------------
struct DiagnosticEvent {
    uint64_t        timestamp;      // monotonic clock ticks
    uint32_t        frame;          // frame counter (0 if not applicable)
    EventType       type;
    seed::ecs::Entity entity;       // affected entity (INVALID_ENTITY if none)
    uint32_t        archetypeHash;  // archetype identifier
    uint32_t        componentType;  // component type id
    uint32_t        index;          // row index within archetype
    const char*     description;    // static string, never freed
    const char*     file;           // __FILE__
    int             line;           // __LINE__

    DiagnosticEvent() noexcept
        : timestamp(0)
        , frame(0)
        , type(EventType::Custom)
        , entity(seed::ecs::INVALID_ENTITY)
        , archetypeHash(0)
        , componentType(0)
        , index(0)
        , description("")
        , file("")
        , line(0)
    {}
};

// ---------------------------------------------------------------------------
// EventTimeline – lock-free SPSC ring buffer for diagnostic events
// ---------------------------------------------------------------------------
// Thread-safety: Single-producer (main thread) / single-consumer (dump thread).
// All ECS operations happen on the main thread in P0-M3.
// ---------------------------------------------------------------------------
class EventTimeline {
public:
    static constexpr size_t Capacity = SEED_DIAGNOSTICS_MAX_EVENTS;

    EventTimeline() noexcept;

    // Push an event. If buffer is full, overwrites oldest (circular).
    void push(EventType type,
              seed::ecs::Entity entity = seed::ecs::INVALID_ENTITY,
              uint32_t archetypeHash = 0,
              uint32_t componentType = 0,
              uint32_t index = 0,
              const char* description = "",
              const char* file = "",
              int line = 0) noexcept;

    // Push a fully-formed event
    void push(const DiagnosticEvent& ev) noexcept;

    // Read all events into a vector (clears internal buffer)
    template<typename OutputIt>
    void drain(OutputIt out) noexcept;

    // Number of events currently stored
    size_t size() const noexcept;

    // Clear all events
    void clear() noexcept;

    // Dump formatted events to a string (for crash reports)
    std::string dump() const;

private:
    alignas(64) std::array<DiagnosticEvent, Capacity> m_buffer;
    alignas(64) std::atomic<size_t> m_writeIdx{0};  // only producer writes
    alignas(64) std::atomic<size_t> m_readIdx{0};   // only consumer reads
};

// ---------------------------------------------------------------------------
// Global timeline instance (initialized on first use)
// ---------------------------------------------------------------------------
EventTimeline& globalTimeline() noexcept;

// ---------------------------------------------------------------------------
// Convenience macros – disabled when timeline is off
// ---------------------------------------------------------------------------
#if SEED_DIAGNOSTICS_EVENT_TIMELINE
#  define SEED_DIAG_EVENT(type, entity, arch, comp, idx, desc, file, line)      ::seed::diagnostics::globalTimeline().push(          (type), (entity), (arch), (comp), (idx), (desc), (file), (line))
#else
#  define SEED_DIAG_EVENT(type, entity, arch, comp, idx, desc, file, line) ((void)0)
#endif

} // namespace seed::diagnostics
