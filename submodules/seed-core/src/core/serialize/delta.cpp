#include "core/serialize/delta.h"
#include "core/serialize/snapshot.h"
#include "core/serialize/binary_writer.h"
#include "core/ecs/world.h"
#include "core/ecs/component_traits.h"
#include "core/ecs/type_registry.h"
#include "core/profiling/tracy_seed.h"
#include "core/profiling/seed_assert.h"
#include <cstring>
#include <unordered_map>
#include "core/serialize/binary_reader.h"

namespace seed::serialize {

std::vector<uint8_t> DeltaCompressor::compute(
    const std::vector<uint8_t>& oldData,
    const std::vector<uint8_t>& newData)
{
    SEED_ZONE("DeltaCompressor::compute");

    if (oldData.empty() || newData.size() <= oldData.size() / 2) {
        BinaryWriter writer;
        Delta::Header header;
        header.flags = 0x1;
        writer.writePOD(header);
        writer.writeUInt32(static_cast<uint32_t>(newData.size()));
        writer.writeBytes(newData.data(), newData.size());
        return writer.data();
    }

    BinaryWriter writer;
    Delta::Header header;
    header.flags = 0x2;
    writer.writePOD(header);

    size_t minSize = (oldData.size() < newData.size()) ? oldData.size() : newData.size();

    std::vector<std::pair<size_t, std::vector<uint8_t>>> ranges;
    bool inRange = false;
    size_t rangeStart = 0;

    for (size_t i = 0; i < minSize; ++i) {
        if (oldData[i] != newData[i]) {
            if (!inRange) {
                rangeStart = i;
                inRange = true;
            }
        } else {
            if (inRange) {
                std::vector<uint8_t> rangeData(newData.data() + rangeStart, newData.data() + i);
                ranges.push_back({rangeStart, std::move(rangeData)});
                inRange = false;
            }
        }
    }
    if (inRange) {
        std::vector<uint8_t> rangeData(newData.data() + rangeStart, newData.data() + minSize);
        ranges.push_back({rangeStart, std::move(rangeData)});
    }

    size_t deltaSize = sizeof(Delta::Header) + 4;
    for (const auto& [start, data] : ranges) {
        deltaSize += 8 + data.size();
    }

    if (deltaSize >= newData.size() / 2) {
        writer.clear();
        Delta::Header fullHeader;
        fullHeader.flags = 0x1;
        writer.writePOD(fullHeader);
        writer.writeUInt32(static_cast<uint32_t>(newData.size()));
        writer.writeBytes(newData.data(), newData.size());
        return writer.data();
    }

    header.numChangedEntities = static_cast<uint32_t>(ranges.size());
    writer.clear();
    writer.writePOD(header);
    writer.writeUInt32(static_cast<uint32_t>(ranges.size()));

    for (const auto& [start, data] : ranges) {
        writer.writeUInt32(static_cast<uint32_t>(start));
        writer.writeUInt32(static_cast<uint32_t>(data.size()));
        writer.writeBytes(data.data(), data.size());
    }

    if (newData.size() > oldData.size()) {
        writer.writeUInt32(static_cast<uint32_t>(minSize));
        writer.writeUInt32(static_cast<uint32_t>(newData.size() - minSize));
        writer.writeBytes(newData.data() + minSize, newData.size() - minSize);
    }

    return writer.data();
}

std::vector<uint8_t> DeltaCompressor::apply(
    const std::vector<uint8_t>& oldData,
    const std::vector<uint8_t>& deltaData)
{
    SEED_ZONE("DeltaCompressor::apply");
    SEED_ASSERT(!deltaData.empty(), "Empty delta data");

    BinaryReader reader(deltaData);
    Delta::Header header = reader.readPOD<Delta::Header>();

    if (header.flags & 0x1) {
        uint32_t size = reader.readUInt32();
        std::vector<uint8_t> result(size);
        reader.readBytes(result.data(), size);
        return result;
    }

    std::vector<uint8_t> result = oldData;
    uint32_t rangeCount = reader.readUInt32();

    for (uint32_t i = 0; i < rangeCount; ++i) {
        uint32_t offset = reader.readUInt32();
        uint32_t size = reader.readUInt32();
        if (offset + size > result.size()) {
            result.resize(offset + size);
        }
        reader.readBytes(result.data() + offset, size);
    }

    return result;
}

std::vector<uint8_t> DeltaCompressor::compressFloatArray(
    const float* oldArray,
    const float* newArray,
    size_t count)
{
    SEED_ZONE("DeltaCompressor::compressFloatArray");

    BinaryWriter writer;
    uint32_t changedCount = 0;

    for (size_t i = 0; i < count; ++i) {
        uint32_t oldBits;
        uint32_t newBits;
        std::memcpy(&oldBits, &oldArray[i], sizeof(oldBits));
        std::memcpy(&newBits, &newArray[i], sizeof(newBits));
        if (oldBits != newBits) ++changedCount;
    }

    writer.writeUInt32(static_cast<uint32_t>(count));
    writer.writeUInt32(changedCount);

    for (size_t i = 0; i < count; ++i) {
        uint32_t oldBits;
        uint32_t newBits;
        std::memcpy(&oldBits, &oldArray[i], sizeof(oldBits));
        std::memcpy(&newBits, &newArray[i], sizeof(newBits));
        if (oldBits != newBits) {
            writer.writeUInt32(static_cast<uint32_t>(i));
            writer.writeUInt32(oldBits ^ newBits);
        }
    }

    return writer.data();
}

void DeltaCompressor::decompressFloatArray(
    const uint8_t* compressed,
    size_t compressedSize,
    const float* oldArray,
    float* outArray,
    size_t count)
{
    SEED_ZONE("DeltaCompressor::decompressFloatArray");
    SEED_ASSERT(compressedSize >= 8, "Invalid compressed float array");

    BinaryReader reader(compressed, compressedSize);
    uint32_t storedCount = reader.readUInt32();
    SEED_ASSERT(storedCount == count, "Float array count mismatch");

    std::memcpy(outArray, oldArray, count * sizeof(float));

    uint32_t changedCount = reader.readUInt32();
    for (uint32_t i = 0; i < changedCount; ++i) {
        uint32_t idx = reader.readUInt32();
        uint32_t xorDelta = reader.readUInt32();
        SEED_ASSERT(idx < count, "Float array index out of bounds");

        uint32_t oldBits;
        std::memcpy(&oldBits, &oldArray[idx], sizeof(oldBits));
        uint32_t newBits = oldBits ^ xorDelta;
        std::memcpy(&outArray[idx], &newBits, sizeof(newBits));
    }
}

Delta::Header Delta::readHeader() const {
    if (m_data.size() < sizeof(Header)) return Header{};
    BinaryReader reader(m_data);
    return reader.readPOD<Header>();
}

void Delta::apply(seed::ecs::World& world) const {
    SEED_ZONE("Delta::apply");
    SEED_ASSERT(!m_data.empty(), "Cannot apply empty delta");

    BinaryReader reader(m_data);
    Header header = reader.readPOD<Header>();

    SEED_ASSERT(header.magic == 0x44454C54, "Invalid delta magic");

    if (header.flags & 0x1) {
        uint32_t snapSize = reader.readUInt32();
        std::vector<uint8_t> snapData(snapSize);
        reader.readBytes(snapData.data(), snapSize);
        auto snap = Snapshot::deserialize(snapData);
        // Clear existing entities to prevent duplicates when applying full snapshot
        std::vector<seed::ecs::Entity> toDestroy;
        for (const auto& [id, arch] : world.archetypeManager()) {
            for (size_t i = 0; i < arch->size(); ++i) {
                toDestroy.push_back(arch->entityAt(i));
            }
        }
        for (auto e : toDestroy) {
            if (world.isAlive(e)) world.destroyEntity(e);
        }
        snap.apply(world);
        return;
    }

    std::unordered_map<uint32_t, seed::ecs::Entity> worldEntities;
    for (const auto& [id, arch] : world.archetypeManager()) {
        for (size_t i = 0; i < arch->size(); ++i) {
            seed::ecs::Entity e = arch->entityAt(i);
            worldEntities[seed::ecs::entityIndex(e)] = e;
        }
    }

    for (uint32_t i = 0; i < header.numChangedEntities; ++i) {
        seed::ecs::Entity deltaEntity = reader.readUInt32();
        uint32_t numChangedComponents = reader.readUInt32();

        uint32_t idx = seed::ecs::entityIndex(deltaEntity);
        auto it = worldEntities.find(idx);
        if (it == worldEntities.end()) continue;

        seed::ecs::Entity worldEntity = it->second;
        if (!world.isAlive(worldEntity)) continue;

        for (uint32_t c = 0; c < numChangedComponents; ++c) {
            seed::ecs::ComponentType ctype = reader.readUInt32();
            uint32_t compFlags = reader.readUInt32();
            uint32_t dataSize = reader.readUInt32();
            std::vector<uint8_t> compData(dataSize);
            reader.readBytes(compData.data(), dataSize);

            const auto& meta = seed::ecs::TypeRegistry::instance().getMeta(ctype);
            if (compFlags == 0x1 && meta.floatCount > 0) {
                // Float-array XOR decompression (Monat 5 Gap Analysis fix)
                auto* oldComp = world.getComponentRaw(worldEntity, ctype);
                std::vector<uint8_t> decompressed(meta.size);
                if (oldComp) {
                    seed::serialize::DeltaCompressor::decompressFloatArray(
                        compData.data(), compData.size(),
                        reinterpret_cast<const float*>(oldComp),
                        reinterpret_cast<float*>(decompressed.data()),
                        meta.floatCount);
                } else {
                    decompressed = std::move(compData);
                }
                world.setComponentRaw(worldEntity, ctype, decompressed.data());
            } else if (compFlags == 0x2 && meta.decompress != nullptr) {
                // Custom decompression hook
                auto* oldComp = world.getComponentRaw(worldEntity, ctype);
                std::vector<uint8_t> decompressed(meta.size);
                meta.decompress(compData, oldComp, decompressed.data(), meta.size);
                world.setComponentRaw(worldEntity, ctype, decompressed.data());
            } else {
                // Raw bytes
                world.setComponentRaw(worldEntity, ctype, compData.data());
            }
        }
    }

    // Remove entities BEFORE adding new ones to allow index recycling
    // (a removed slot may be reused by a new entity with a higher version)
    for (uint32_t i = 0; i < header.numRemovedEntities; ++i) {
        seed::ecs::Entity removedEntity = reader.readUInt32();
        uint32_t oldCompCount = reader.readUInt32();
        // Skip the component-type list written by computeDelta for removed entities
        for (uint32_t j = 0; j < oldCompCount; ++j) {
            (void)reader.readUInt32();
        }
        uint32_t idx = seed::ecs::entityIndex(removedEntity);
        auto it = worldEntities.find(idx);
        if (it != worldEntities.end() && world.isAlive(it->second)) {
            world.destroyEntity(it->second);
        }
    }

    for (uint32_t i = 0; i < header.numNewEntities; ++i) {
        seed::ecs::Entity storedEntity = reader.readUInt32();
        uint32_t numComponents = reader.readUInt32();

        seed::ecs::Entity newEntity = world.createEntityWithId(storedEntity);

        for (uint32_t c = 0; c < numComponents; ++c) {
            seed::ecs::ComponentType ctype = reader.readUInt32();
            (void)reader.readUInt32(); // compFlags - read but unused (no compression flags yet)
            uint32_t dataSize = reader.readUInt32();
            std::vector<uint8_t> compData(dataSize);
            reader.readBytes(compData.data(), dataSize);
            world.addComponentRaw(newEntity, ctype, compData.data());
        }
    }
}

std::vector<uint8_t> Delta::serialize() const {
    return m_data;
}

Delta Delta::deserialize(const std::vector<uint8_t>& data) {
    return Delta(data);
}

} // namespace seed::serialize
