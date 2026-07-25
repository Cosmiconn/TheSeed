#pragma once

#include "core/serialize/binary_writer.h"
#include "core/ecs/entity.h"
#include "core/ecs/component_traits.h"
#include <vector>
#include <cstdint>

namespace seed::ecs {
    class World;
}

namespace seed::serialize {

class Delta;

struct SnapshotHeader {
    static constexpr uint32_t MAGIC = 0x53454544; // "SEED"
    static constexpr uint32_t VERSION = 1;

    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t entityCount = 0;
    uint32_t archetypeCount = 0;
    uint64_t timestampUs = 0;
    uint32_t schemaVersion = 1;
};

class Snapshot {
public:
    Snapshot() = default;
    explicit Snapshot(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    static Snapshot capture(const seed::ecs::World& world);
    void apply(seed::ecs::World& world) const;

    std::vector<uint8_t> serialize() const;
    static Snapshot deserialize(const std::vector<uint8_t>& data);

    const std::vector<uint8_t>& data() const { return m_data; }
    size_t size() const { return m_data.size(); }
    uint64_t timestampUs() const;

    Delta computeDelta(const Snapshot& older) const;

    struct EntityState {
        seed::ecs::Entity entity;
        std::vector<seed::ecs::ComponentType> types;
        std::vector<std::vector<uint8_t>> componentData;
    };

    std::vector<EntityState> parseEntities() const;

private:
    std::vector<uint8_t> m_data;
};

} // namespace seed::serialize
