#include "core/diagnostics/event_timeline.h"
#include <chrono>
#include <iterator>
#include <vector>
#include <fmt/format.h>

namespace seed::diagnostics {

EventTimeline::EventTimeline() noexcept {
    m_buffer.fill(DiagnosticEvent{});
}

void EventTimeline::push(EventType type,
                         seed::ecs::Entity entity,
                         uint32_t archetypeHash,
                         uint32_t componentType,
                         uint32_t index,
                         const char* description,
                         const char* file,
                         int line) noexcept {
    DiagnosticEvent ev;
    ev.timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    ev.frame = 0; // TODO: integrate with FrameTimer
    ev.type = type;
    ev.entity = entity;
    ev.archetypeHash = archetypeHash;
    ev.componentType = componentType;
    ev.index = index;
    ev.description = description;
    ev.file = file;
    ev.line = line;
    push(ev);
}

void EventTimeline::push(const DiagnosticEvent& ev) noexcept {
    const size_t idx = m_writeIdx.fetch_add(1, std::memory_order_relaxed) % Capacity;
    m_buffer[idx] = ev;
    // Simple overwrite semantics: if we lap the reader, reader loses old events.
    // For diagnostics this is acceptable (we keep the most recent).
}

template<typename OutputIt>
void EventTimeline::drain(OutputIt out) noexcept {
    const size_t writePos = m_writeIdx.load(std::memory_order_relaxed);
    const size_t readPos = m_readIdx.load(std::memory_order_relaxed);
    const size_t count = (writePos > readPos) ? (writePos - readPos) : 0;
    const size_t toRead = (count > Capacity) ? Capacity : count;

    for (size_t i = 0; i < toRead; ++i) {
        const size_t idx = (readPos + i) % Capacity;
        *out++ = m_buffer[idx];
    }
    m_readIdx.store(readPos + toRead, std::memory_order_relaxed);
}

// Explicit instantiation for common containers
template void EventTimeline::drain<std::back_insert_iterator<std::vector<DiagnosticEvent>>>(
    std::back_insert_iterator<std::vector<DiagnosticEvent>>) noexcept;

size_t EventTimeline::size() const noexcept {
    const size_t writePos = m_writeIdx.load(std::memory_order_relaxed);
    const size_t readPos = m_readIdx.load(std::memory_order_relaxed);
    const size_t count = (writePos > readPos) ? (writePos - readPos) : 0;
    // BUGFIX: Der Ringpuffer kann physisch nie mehr als 'Capacity' gueltige
    // Eintraege halten - ueberschreibende push()-Aufrufe verwerfen aeltere
    // Events, ohne m_readIdx nachzuziehen. Ungekappt lieferte size() hier
    // die rohe Differenz (writePos - readPos), die bei mehr als Capacity
    // Pushes ohne zwischenzeitliches drain()/clear() > Capacity werden
    // konnte (CI: EventTimeline_Overflow, 10100 <= 10000 schlug fehl).
    return (count > Capacity) ? Capacity : count;
}

void EventTimeline::clear() noexcept {
    m_readIdx.store(m_writeIdx.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

std::string EventTimeline::dump() const {
    std::string out;
    out.reserve(size() * 128);

    const size_t writePos = m_writeIdx.load(std::memory_order_relaxed);
    const size_t readPos = m_readIdx.load(std::memory_order_relaxed);
    const size_t count = (writePos > readPos) ? (writePos - readPos) : 0;
    const size_t toRead = (count > Capacity) ? Capacity : count;

    for (size_t i = 0; i < toRead; ++i) {
        const size_t idx = (readPos + i) % Capacity;
        const auto& ev = m_buffer[idx];
        fmt::format_to(std::back_inserter(out),
            "[{:>12}] {:>24} | E={:08x} A={:08x} C={:04x} I={:4} | {}:{} | {}\n",
            ev.timestamp,
            eventTypeToString(ev.type),
            ev.entity,
            ev.archetypeHash,
            ev.componentType,
            ev.index,
            ev.file ? ev.file : "?",
            ev.line,
            ev.description ? ev.description : ""
        );
    }
    return out;
}

EventTimeline& globalTimeline() noexcept {
    static EventTimeline s_timeline;
    return s_timeline;
}

} // namespace seed::diagnostics
