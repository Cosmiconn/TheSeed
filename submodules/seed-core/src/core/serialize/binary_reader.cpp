#include "core/serialize/binary_reader.h"
#include "core/profiling/seed_assert.h"
#include <bit>
#include <stdexcept>

namespace seed::serialize {

void BinaryReader::ensureRemaining(size_t bytes) const {
    SEED_ASSERT(m_offset + bytes <= m_size,
                "BinaryReader: not enough data remaining");
}

uint8_t BinaryReader::readUInt8() {
    ensureRemaining(1);
    return m_data[m_offset++];
}

uint16_t BinaryReader::readUInt16() {
    ensureRemaining(2);
    uint16_t value;
    std::memcpy(&value, &m_data[m_offset], sizeof(value));
    m_offset += sizeof(value);
    if constexpr (std::endian::native == std::endian::big) {
        value = static_cast<uint16_t>(
            ((value & 0x00FFu) << 8) |
            ((value & 0xFF00u) >> 8));
    }
    return value;
}

uint32_t BinaryReader::readUInt32() {
    ensureRemaining(4);
    uint32_t value;
    std::memcpy(&value, &m_data[m_offset], sizeof(value));
    m_offset += sizeof(value);
    if constexpr (std::endian::native == std::endian::big) {
        value = ((value & 0x000000FFu) << 24) |
                ((value & 0x0000FF00u) <<  8) |
                ((value & 0x00FF0000u) >>  8) |
                ((value & 0xFF000000u) >> 24);
    }
    return value;
}

uint64_t BinaryReader::readUInt64() {
    ensureRemaining(8);
    uint64_t value;
    std::memcpy(&value, &m_data[m_offset], sizeof(value));
    m_offset += sizeof(value);
    if constexpr (std::endian::native == std::endian::big) {
        value = ((value & 0x00000000000000FFull) << 56) |
                ((value & 0x000000000000FF00ull) << 40) |
                ((value & 0x0000000000FF0000ull) << 24) |
                ((value & 0x00000000FF000000ull) <<  8) |
                ((value & 0x000000FF00000000ull) >>  8) |
                ((value & 0x0000FF0000000000ull) >> 24) |
                ((value & 0x00FF000000000000ull) >> 40) |
                ((value & 0xFF00000000000000ull) >> 56);
    }
    return value;
}

float BinaryReader::readFloat() {
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    uint32_t bits = readUInt32();
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double BinaryReader::readDouble() {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    uint64_t bits = readUInt64();
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void BinaryReader::readBytes(void* out, size_t size) {
    if (size == 0) return;
    SEED_ASSERT(out != nullptr, "dest is null");
    ensureRemaining(size);
    std::memcpy(out, &m_data[m_offset], size);
    m_offset += size;
}

std::string BinaryReader::readString() {
    uint32_t len = readUInt32();
    ensureRemaining(len);
    std::string result;
    result.resize(len);
    if (len > 0) {
        std::memcpy(result.data(), &m_data[m_offset], len);
        m_offset += len;
    }
    return result;
}

} // namespace seed::serialize
