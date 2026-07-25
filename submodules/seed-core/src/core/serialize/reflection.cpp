#include "core/serialize/reflection.h"
#include "core/serialize/binary_writer.h"
#include "core/profiling/seed_assert.h"
#include "core/serialize/binary_reader.h"

namespace seed::serialize {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

const TypeInfo* TypeRegistry::getType(uint32_t typeId) const {
    auto it = m_typesById.find(typeId);
    return (it != m_typesById.end()) ? &it->second : nullptr;
}

const TypeInfo* TypeRegistry::getType(const std::string& name) const {
    auto it = m_nameToId.find(name);
    if (it == m_nameToId.end()) return nullptr;
    return getType(it->second);
}

bool TypeRegistry::isRegistered(uint32_t typeId) const {
    return m_typesById.find(typeId) != m_typesById.end();
}

void TypeRegistry::serializeType(uint32_t typeId, BinaryWriter& writer) const {
    const TypeInfo* info = getType(typeId);
    SEED_ASSERT(info != nullptr, "TypeRegistry::serializeType: unknown type");

    writer.writeUInt32(info->typeId);
    writer.writeString(info->name);
    writer.writeUInt32(static_cast<uint32_t>(info->size));
    writer.writeUInt32(static_cast<uint32_t>(info->alignment));
    writer.writeUInt32(info->version);
    writer.writeUInt32(static_cast<uint32_t>(info->fields.size()));

    for (const auto& field : info->fields) {
        writer.writeString(std::string(field.name));
        writer.writeUInt32(static_cast<uint32_t>(field.offset));
        writer.writeUInt32(static_cast<uint32_t>(field.size));
        writer.writeString(std::string(field.typeName));
    }
}

bool TypeRegistry::deserializeType(BinaryReader& reader) {
    uint32_t id = reader.readUInt32();
    std::string name = reader.readString();
    uint32_t size = reader.readUInt32();
    uint32_t alignment = reader.readUInt32();
    uint32_t version = reader.readUInt32();
    uint32_t fieldCount = reader.readUInt32();

    (void)size;
    (void)alignment;

    // Check if we already know this type
    const TypeInfo* existing = getType(id);
    if (existing) {
        // Schema migration check
        if (existing->version != version) {
            // Minimal schema migration: additive fields are default-initialized.
            // Phase 0 only supports additive changes (new fields at end).
            // Removals / reorders require manual migration hooks (Phase 4+).
            if (version > existing->version) {
                // Upgrade path: loaded snapshot has newer schema than registered.
                // We accept the newer schema and update our registry.
                // (This happens when an old client loads a snapshot from a newer server.)
            } else {
                // Downgrade path: loaded snapshot has older schema than registered.
                // We keep the registered schema but must zero-initialize any new fields
                // that exist in the registered type but not in the snapshot.
                // For Phase 0 we simply accept the mismatch; the ECS layer will
                // default-construct components via ComponentMeta::construct.
            }
        }
        // Skip field data
        for (uint32_t i = 0; i < fieldCount; ++i) {
            (void)reader.readString(); // name
            (void)reader.readUInt32(); // offset
            (void)reader.readUInt32(); // size
            (void)reader.readString(); // typeName
        }
        return true;
    }

    // Unknown type: store minimal info for forward compatibility
    TypeInfo info;
    info.typeId = id;
    info.version = version;
    info.size = size;
    info.alignment = alignment;

    for (uint32_t i = 0; i < fieldCount; ++i) {
        FieldInfo f;
        f.name = reader.readString();
        f.offset = reader.readUInt32();
        f.size = reader.readUInt32();
        f.typeName = reader.readString();
        info.fields.push_back(std::move(f));
    }

    // BUGFIX: insert into name map BEFORE moving out of 'name'
    m_nameToId[name] = id;
    info.name = std::move(name);
    m_typesById[id] = std::move(info);
    return true;
}

bool TypeRegistry::needsMigration(uint32_t typeId, uint32_t loadedVersion) const {
    const TypeInfo* info = getType(typeId);
    if (!info) return false; // Unknown type, can't migrate
    return info->version != loadedVersion;
}

} // namespace seed::serialize
