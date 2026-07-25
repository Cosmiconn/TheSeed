#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <type_traits>
#include <bit>

namespace seed::serialize {

class BinaryWriter {
public:
    BinaryWriter() = default;
    explicit BinaryWriter(size_t reserveBytes) { m_buffer.reserve(reserveBytes); }

    void writeUInt8(uint8_t v);
    void writeUInt16(uint16_t v);
    void writeUInt32(uint32_t v);
    void writeUInt64(uint64_t v);
    void writeInt8(int8_t v)   { writeUInt8(static_cast<uint8_t>(v)); }
    void writeInt16(int16_t v) { writeUInt16(static_cast<uint16_t>(v)); }
    void writeInt32(int32_t v) { writeUInt32(static_cast<uint32_t>(v)); }
    void writeInt64(int64_t v) { writeUInt64(static_cast<uint64_t>(v)); }
    void writeFloat(float v);
    void writeDouble(double v);
    void writeBool(bool v)     { writeUInt8(v ? 1u : 0u); }

    void writeBytes(const void* data, size_t size);
    void writeString(const std::string& s);

    template<typename T>
    void writePOD(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "writePOD requires trivially copyable type");
        writeBytes(&value, sizeof(T));
    }

    const std::vector<uint8_t>& data() const { return m_buffer; }
    std::vector<uint8_t>& data() { return m_buffer; }
    size_t size() const { return m_buffer.size(); }
    void clear() { m_buffer.clear(); }

private:
    std::vector<uint8_t> m_buffer;
};

} // namespace seed::serialize
