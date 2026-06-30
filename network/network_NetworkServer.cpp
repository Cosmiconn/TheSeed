// =============================================================================
// network/network_NetworkServer.cpp — High-Level Network Server (AP-32)
// =============================================================================
// KORREKTUR: <span> Header hinzugefügt für std::span (C++23)
// =============================================================================
#include "network_NetworkServer.h"
#include "../core/Log.h"

#include <span>      // C++23: std::span
#include <cstring>
#include <chrono>

namespace net {

// =============================================================================
// Session Management
// =============================================================================
bool NetworkServer::Start(uint16_t port) {
    if (!udpSocket.Create(port)) {
        AddLog("[Net] UDP Socket konnte nicht auf Port {} erstellt werden", port);
        return false;
    }

    AddLog("[Net] NetworkServer gestartet auf Port {}", port);
    return true;
}

void NetworkServer::Stop() {
    udpSocket.Close();
    AddLog("[Net] NetworkServer gestoppt.");
}

// =============================================================================
// Paketverarbeitung
// =============================================================================
void NetworkServer::ProcessIncoming() {
    std::vector<uint8_t> buffer(2048);
    std::string senderIp;
    uint16_t senderPort = 0;

    // Non-blocking Empfang
    int received = udpSocket.ReceiveFrom(buffer, senderIp, senderPort);
    if (received <= 0) return;

    if (static_cast<size_t>(received) < sizeof(PacketHeader)) {
        AddLog("[Net] Paket zu klein ({} Bytes), verworfen", received);
        return;
    }

    // Header parsen
    PacketHeader header;
    std::memcpy(&header, buffer.data(), sizeof(PacketHeader));

    // Protokoll-ID prüfen
    if (header.protocolId != 0x4D4D) {
        AddLog("[Net] Ungültige Protokoll-ID ({:04X}), verworfen", header.protocolId);
        return;
    }

    // Payload extrahieren
    std::span<uint8_t> payload(buffer.data() + sizeof(PacketHeader),
                                static_cast<size_t>(received) - sizeof(PacketHeader));

    // ACK-Verarbeitung
    ProcessAck(header.ack, header.ackBitmap);

    // Reliable-Pakete in Queue
    if (static_cast<uint16_t>(header.flags) & static_cast<uint16_t>(PacketFlags::Reliable)) {
        if (!IsSequenceAcked(header.sequence)) {
            QueueReliablePacket(header.sequence, payload, senderIp, senderPort);
        }
    }

    // An Callback weiterleiten
    if (packetCallback) {
        packetCallback(header, payload, senderIp, senderPort);
    }
}

// =============================================================================
// Senden
// =============================================================================
void NetworkServer::SendPacket(const PacketHeader& header,
                                std::span<const uint8_t> payload,
                                std::string_view ip,
                                uint16_t port) {
    std::vector<uint8_t> packet(sizeof(PacketHeader) + payload.size());
    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    if (!payload.empty()) {
        std::memcpy(packet.data() + sizeof(PacketHeader), payload.data(), payload.size());
    }

    udpSocket.SendTo(std::span(packet), ip, port);
}

void NetworkServer::SendReliable(const PacketHeader& header,
                                  std::span<const uint8_t> payload,
                                  std::string_view ip,
                                  uint16_t port) {
    // Speichere für Retransmission
    PendingPacket pending;
    pending.header = header;
    pending.payload.assign(payload.begin(), payload.end());
    pending.destinationIp = std::string(ip);
    pending.destinationPort = port;
    pending.sendTime = std::chrono::steady_clock::now();
    pending.retryCount = 0;

    pendingPackets.push_back(std::move(pending));

    // Sofort senden
    SendPacket(header, payload, ip, port);
}

// =============================================================================
// Retransmission
// =============================================================================
void NetworkServer::ProcessRetransmissions() {
    auto now = std::chrono::steady_clock::now();

    for (auto& pending : pendingPackets) {
        if (pending.acked) continue;

        auto elapsed = std::chrono::duration<float>(now - pending.sendTime).count();
        if (elapsed > rttEstimator.GetRto()) {
            if (pending.retryCount >= MAX_RETRIES) {
                AddLog("[Net] Paket {} nach {} Versuchen verworfen",
                       pending.header.sequence, MAX_RETRIES);
                pending.acked = true; // Markiere als erledigt
                continue;
            }

            // Retransmit
            pending.retryCount++;
            pending.sendTime = now;
            SendPacket(pending.header,
                       std::span(pending.payload),
                       pending.destinationIp,
                       pending.destinationPort);

            AddLog("[Net] Retransmission #{} für Sequenz {} (RTO={:.3f}ms)",
                   pending.retryCount, pending.header.sequence,
                   rttEstimator.GetRto() * 1000.0f);
        }
    }

    // Bereinigung: Entferne bestätigte Pakete
    std::erase_if(pendingPackets, [](const PendingPacket& p) { return p.acked; });
}

// =============================================================================
// ACK-Verarbeitung
// =============================================================================
void NetworkServer::ProcessAck(uint16_t ack, uint32_t ackBitmap) {
    for (auto& pending : pendingPackets) {
        if (pending.acked) continue;

        uint16_t seq = pending.header.sequence;
        uint16_t diff = static_cast<uint16_t>(ack - seq);

        if (diff == 0) {
            // Direktes ACK
            pending.acked = true;
        } else if (diff > 0 && diff <= 32) {
            // Bitmap-ACK
            uint32_t bit = 1u << (diff - 1);
            if (ackBitmap & bit) {
                pending.acked = true;
            }
        }
    }
}

bool NetworkServer::IsSequenceAcked(uint16_t sequence) const {
    for (const auto& pending : pendingPackets) {
        if (pending.header.sequence == sequence && !pending.acked) {
            return false;
        }
    }
    return true;
}

void NetworkServer::QueueReliablePacket(uint16_t sequence,
                                         std::span<const uint8_t> payload,
                                         std::string_view ip,
                                         uint16_t port) {
    (void)sequence; (void)payload; (void)ip; (void)port;
    // TODO: In Empfangs-Queue einreihen, an Applikation weiterleiten
}

} // namespace net
