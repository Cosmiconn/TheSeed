#include "core/serialize/binary_writer.h"
#include "core/profiling/seed_assert.h"
#include <bit>
#include <cstring>

namespace seed::serialize {

namespace {
    constexpr uint16_t toLittleEndian(uint16_t value) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return static_cast<uint16_t>(
                ((value & 0x00FFu) << 8) |
                ((value & 0xFF00u) >> 8));
        } else {
            return value;
        }
    }

    constexpr uint32_t toLittleEndian(uint32_t value) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return ((value & 0x000000FFu) << 24) |
                   ((value & 0x0000FF00u) <<  8) |
                   ((value & 0x00FF0000u) >>  8) |
                   ((value & 0xFF000000u) >> 24);
        } else {
            return value;
        }
    }

    constexpr uint64_t toLittleEndian(uint64_t value) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return ((value & 0x00000000000000FFull) << 56) |
                   ((value & 0x000000000000FF00ull) << 40) |
                   ((value & 0x0000000000FF0000ull) << 24) |
                   ((value & 0x00000000FF000000ull) <<  8) |
                   ((value & 0x000000FF00000000ull) >>  8) |
                   ((value & 0x0000FF0000000000ull) >> 24) |
                   ((value & 0x00FF000000000000ull) >> 40) |
                   ((value & 0xFF00000000000000ull) >> 56);
        } else {
            return value;
        }
    }
}

void BinaryWriter::writeUInt8(uint8_t value) {
    m_buffer.push_back(value);
}

void BinaryWriter::writeUInt16(uint16_t value) {
    uint16_t le = toLittleEndian(value);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&le);
    m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(le));
}

void BinaryWriter::writeUInt32(uint32_t value) {
    uint32_t le = toLittleEndian(value);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&le);
    m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(le));
}

void BinaryWriter::writeUInt64(uint64_t value) {
    uint64_t le = toLittleEndian(value);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&le);
    m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(le));
}

void BinaryWriter::writeFloat(float value) {
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUInt32(bits);
}

void BinaryWriter::writeDouble(double value) {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUInt64(bits);
}

void BinaryWriter::writeBytes(const void* data, size_t size) {
    SEED_ASSERT(data != nullptr || size == 0, "data is null but size > 0");
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_buffer.insert(m_buffer.end(), bytes, bytes + size);
}

void BinaryWriter::writeString(const std::string& str) {
    writeUInt32(static_cast<uint32_t>(str.size()));
    if (!str.empty()) {
        writeBytes(str.data(), str.size());
    }
}

} // namespace seed::serialize
