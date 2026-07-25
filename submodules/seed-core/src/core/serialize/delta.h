#pragma once

#include "core/serialize/binary_writer.h"
#include "core/ecs/entity.h"
#include <vector>
#include <cstdint>

namespace seed::ecs {
    class World;
}

namespace seed::serialize {

class DeltaCompressor {
public:
    static std::vector<uint8_t> compute(
        const std::vector<uint8_t>& oldData,
        const std::vector<uint8_t>& newData);

    static std::vector<uint8_t> apply(
        const std::vector<uint8_t>& oldData,
        const std::vector<uint8_t>& deltaData);

    static std::vector<uint8_t> compressFloatArray(
        const float* oldArray,
        const float* newArray,
        size_t count);

    static void decompressFloatArray(
        const uint8_t* compressed,
        size_t compressedSize,
        const float* oldArray,
        float* outArray,
        size_t count);
};

class Delta {
public:
    Delta() = default;
    explicit Delta(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    void apply(seed::ecs::World& world) const;

    std::vector<uint8_t> serialize() const;
    static Delta deserialize(const std::vector<uint8_t>& data);

    size_t size() const { return m_data.size(); }

    struct Header {
        uint32_t magic = 0x44454C54; // "DELT"
        uint32_t version = 1;
        uint32_t baseSnapshotId = 0;
        uint32_t newSnapshotId = 0;
        uint32_t numChangedEntities = 0;
        uint32_t numNewEntities = 0;
        uint32_t numRemovedEntities = 0;
        uint32_t flags = 0;
    };

    Header readHeader() const;

private:
    std::vector<uint8_t> m_data;
};

} // namespace seed::serialize
