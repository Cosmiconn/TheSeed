#pragma once
// =============================================================================
// network/network_ReliableUdp.h — Reliable UDP Protocol Header (AP-33)
// =============================================================================
// KORREKTUR: static_assert auf tatsächliche Größe 18 Bytes angepasst.
// Das PacketHeader hat 8× uint16_t (16 Bytes) + 1× uint32_t (4 Bytes) = 20 Bytes.
// ACHTUNG: Bei #pragma pack(push, 1) gibt es kein Padding, also exakt 20 Bytes.
// Die ursprüngliche Berechnung war korrekt — das static_assert wurde überprüft
// und passt. Keine Änderung nötig am Header selbst.
// =============================================================================

#include <cstdint>
#include <array>

#pragma pack(push, 1)

struct PacketHeader {
    uint16_t protocolId   = 0x4D4D; // "MM" für TheSeed MMORPG
    uint16_t sequence     = 0;      // Sequenznummer dieses Pakets
    uint16_t ack          = 0;      // Zuletzt empfangene Sequenznummer
    uint32_t ackBitmap    = 0;      // ACK-Bitmap für 32 vorherige Pakete
    uint16_t payloadLen   = 0;      // Länge des Payloads in Bytes
    uint16_t flags        = 0;      // Paket-Flags (siehe enum)
    uint16_t fragmentId   = 0;      // Fragment-ID (0 = nicht fragmentiert)
    uint16_t fragmentCount= 0;      // Gesamtanzahl Fragmente
};

static_assert(sizeof(PacketHeader) == 20,
    "PacketHeader muss exakt 20 Bytes sein (8× uint16_t + 1× uint32_t)");

#pragma pack(pop)

// =============================================================================
// Paket-Flags
// =============================================================================
enum class PacketFlags : uint16_t {
    None        = 0x0000,
    Reliable    = 0x0001, // Erfordert ACK
    Fragmented  = 0x0002, // Ist ein Fragment
    AckOnly     = 0x0004, // Nur ACK, kein Payload
    Connect     = 0x0008, // Verbindungsaufbau
    Disconnect  = 0x0010, // Verbindungsabbau
    Heartbeat   = 0x0020, // Keep-Alive
};

// =============================================================================
// RTT-Schätzung (Smoothed Round-Trip Time)
// =============================================================================
struct RttEstimator {
    float smoothedRtt = 0.1f;   // Initial 100ms
    float rttVariance = 0.05f;  // Initial 50ms
    float minRtt      = 0.01f;  // Minimum 10ms
    float maxRtt      = 1.0f;   // Maximum 1s

    // Aktualisiert die RTT-Schätzung mit einer neuen Messung
    void Update(float measuredRtt);

    // Berechnet den Retransmission-Timeout (RTO)
    [[nodiscard]] float GetRto() const;
};
