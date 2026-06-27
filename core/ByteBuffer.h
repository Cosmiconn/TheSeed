#pragma once
// =============================================================================
// core/ByteBuffer.h  —  C++23 Modernized
// std::byteswap, std::span, std::format-Fehlermeldungen
// =============================================================================
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <bit>          // std::byteswap (C++23)
#include <span>
#include <format>

struct ByteBuffer {
    std::vector<uint8_t> data;
    size_t               readPos = 0;

    ByteBuffer() = default;
    explicit ByteBuffer(std::span<const uint8_t> d) : data(d.begin(), d.end()), readPos(0) {}

    // --- Schreiben -----------------------------------------------------------
    void WriteUInt8(uint8_t v) { data.push_back(v); }

    void WriteUInt32(uint32_t v) {
        const auto be = std::byteswap(v);   // C++23: host → big-endian
        data.push_back(static_cast<uint8_t>(be >> 24));
        data.push_back(static_cast<uint8_t>(be >> 16));
        data.push_back(static_cast<uint8_t>(be >>  8));
        data.push_back(static_cast<uint8_t>(be      ));
    }

    void WriteFloat(float v) {
        uint32_t tmp; std::memcpy(&tmp, &v, sizeof(float));
        WriteUInt32(tmp);
    }

    void WriteString(std::string_view s) {
        WriteUInt32(static_cast<uint32_t>(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    }

    // --- Lesen ---------------------------------------------------------------
    [[nodiscard]] uint8_t ReadUInt8() {
        if (readPos + 1 > data.size())
            throw std::runtime_error(std::format("[Net] Underflow u8 @ pos {}", readPos));
        return data[readPos++];
    }

    [[nodiscard]] uint32_t ReadUInt32() {
        if (readPos + 4 > data.size())
            throw std::runtime_error(std::format("[Net] Underflow u32 @ pos {}", readPos));
        uint32_t v =
            (static_cast<uint32_t>(data[readPos    ]) << 24) |
            (static_cast<uint32_t>(data[readPos + 1]) << 16) |
            (static_cast<uint32_t>(data[readPos + 2]) <<  8) |
             static_cast<uint32_t>(data[readPos + 3]);
        readPos += 4;
        return std::byteswap(v);   // C++23: big-endian → host
    }

    [[nodiscard]] float ReadFloat() {
        uint32_t tmp = ReadUInt32();
        float v; std::memcpy(&v, &tmp, sizeof(float));
        return v;
    }

    [[nodiscard]] std::string ReadString() {
        uint32_t len = ReadUInt32();
        if (readPos + len > data.size())
            throw std::runtime_error(std::format("[Net] Underflow str @ pos {}", readPos));
        std::string s(data.begin() + readPos, data.begin() + readPos + len);
        readPos += len;
        return s;
    }
};
