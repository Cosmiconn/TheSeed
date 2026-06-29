// =============================================================================
// network/NetworkServer.cpp — UDP Server Implementation (AP-32 + AP-33)
// =============================================================================
#include "network_NetworkServer.h"
#include "../core/Log.h"
#include <cstring>

namespace net {

bool NetworkServer::Startup(uint16_t listenPort, size_t maxCli) {
    maxClients = maxCli;
    port = listenPort;

    if (!socket.Create(true)) {  // IPv6 dual-stack
        AddLog("[NetworkServer] Failed to create socket");
        return false;
    }

    if (!socket.SetNonBlocking(true)) {
        AddLog("[NetworkServer] Failed to set non-blocking");
        return false;
    }

    if (!socket.SetReuseAddr(true)) {
        AddLog("[NetworkServer] Failed to set reuse addr");
    }

    // Large buffers for MMORPG scale
    socket.SetRecvBufferSize(4 * 1024 * 1024);   // 4MB
    socket.SetSendBufferSize(4 * 1024 * 1024);  // 4MB

    Endpoint bindEp = Endpoint::Any(listenPort, true);
    if (!socket.Bind(bindEp)) {
        AddLog("[NetworkServer] Failed to bind to port {}", listenPort);
        return false;
    }

    running = true;
    AddLog("[NetworkServer] Listening on UDP port {} (max clients: {})", listenPort, maxClients);
    return true;
}

void NetworkServer::Shutdown() {
    running = false;

    std::lock_guard lock(clientsMutex);
    for (auto& [id, client] : clients) {
        if (client.channel) {
            PacketHeader disconnectHeader;
            disconnectHeader.flags = PacketHeader::FLAG_DISCONNECT;
            client.channel->SendPacket(disconnectHeader, {});
        }
    }
    clients.clear();
    endpointToClientId.clear();

    socket.Close();
    AddLog("[NetworkServer] Shutdown complete");
}

void NetworkServer::Poll() {
    if (!running) return;

    static thread_local std::vector<uint8_t> recvBuffer(65536);

    // Process up to 1000 packets per tick to prevent starvation
    for (int i = 0; i < 1000; ++i) {
        Endpoint sender;
        auto recvd = socket.RecvFrom(recvBuffer, sender);

        if (!recvd) {
            // Socket error
            break;
        }
        if (*recvd == 0) {
            // No more data
            break;
        }
        if (*recvd < 0) {
            break;
        }

        auto packetData = std::span(recvBuffer.data(), *recvd);

        // Check for connection request (new client)
        if (packetData.size() >= sizeof(PacketHeader)) {
            PacketHeader header;
            std::memcpy(&header, packetData.data(), sizeof(PacketHeader));

            if (header.flags & PacketHeader::FLAG_CONNECT) {
                uint32_t clientId = GetOrCreateClientId(sender);
                AddLog("[NetworkServer] New client connected: {} from {}", clientId, sender.ToString());

                if (connectHandler) {
                    connectHandler(clientId);
                }
                continue;
            }
        }

        // Route to existing client
        std::string epKey = sender.ToString();
        uint32_t clientId = 0;
        {
            std::lock_guard lock(clientsMutex);
            auto it = endpointToClientId.find(epKey);
            if (it != endpointToClientId.end()) {
                clientId = it->second;
            }
        }

        if (clientId == 0) {
            // Unknown endpoint, might be a new client trying to connect without FLAG_CONNECT
            clientId = GetOrCreateClientId(sender);
            AddLog("[NetworkServer] Auto-registered client: {} from {}", clientId, sender.ToString());
            if (connectHandler) {
                connectHandler(clientId);
            }
        }

        // Process through reliable channel
        ClientConnection* client = nullptr;
        {
            std::lock_guard lock(clientsMutex);
            auto it = clients.find(clientId);
            if (it != clients.end()) {
                client = &it->second;
            }
        }

        if (client && client->channel) {
            client->lastRecvTime = std::chrono::steady_clock::now();
            client->packetsRecv++;
            client->bytesRecv += *recvd;

            if (client->channel->ProcessIncoming(packetData, sender)) {
                // Check for complete messages
                while (auto msg = client->channel->PopMessage()) {
                    if (packetHandler) {
                        packetHandler(clientId, std::span(msg->data(), msg->size()));
                    }
                }
            }
        }
    }

    // Update all channels (retransmissions, etc.)
    {
        std::lock_guard lock(clientsMutex);
        for (auto& [id, client] : clients) {
            if (client.channel) {
                client.channel->Update(0.016f); // Assume 60Hz tick
            }
        }
    }

    // Cleanup timed out clients
    CleanupTimedOutClients();
}

bool NetworkServer::SendToClient(uint32_t clientId, std::span<const uint8_t> payload, bool reliable) {
    std::lock_guard lock(clientsMutex);
    auto it = clients.find(clientId);
    if (it == clients.end() || !it->second.channel) {
        return false;
    }

    bool sent = reliable 
        ? it->second.channel->SendReliable(payload)
        : it->second.channel->SendUnreliable(payload);

    if (sent) {
        it->second.packetsSent++;
        it->second.bytesSent += payload.size();
    }

    return sent;
}

void NetworkServer::Broadcast(std::span<const uint8_t> payload, bool reliable) {
    std::lock_guard lock(clientsMutex);
    for (auto& [id, client] : clients) {
        if (client.channel) {
            bool sent = reliable 
                ? client.channel->SendReliable(payload)
                : client.channel->SendUnreliable(payload);
            if (sent) {
                client.packetsSent++;
                client.bytesSent += payload.size();
            }
        }
    }
}

void NetworkServer::BroadcastExcept(uint32_t excludeClientId, std::span<const uint8_t> payload, bool reliable) {
    std::lock_guard lock(clientsMutex);
    for (auto& [id, client] : clients) {
        if (id == excludeClientId) continue;
        if (client.channel) {
            bool sent = reliable 
                ? client.channel->SendReliable(payload)
                : client.channel->SendUnreliable(payload);
            if (sent) {
                client.packetsSent++;
                client.bytesSent += payload.size();
            }
        }
    }
}

void NetworkServer::DisconnectClient(uint32_t clientId) {
    {
        std::lock_guard lock(clientsMutex);
        auto it = clients.find(clientId);
        if (it != clients.end()) {
            // Send disconnect packet
            if (it->second.channel) {
                PacketHeader header;
                header.flags = PacketHeader::FLAG_DISCONNECT;
                it->second.channel->SendPacket(header, {});
            }

            endpointToClientId.erase(it->second.endpoint.ToString());
            clients.erase(it);
        }
    }

    if (disconnectHandler) {
        disconnectHandler(clientId);
    }

    AddLog("[NetworkServer] Client {} disconnected", clientId);
}

bool NetworkServer::IsClientConnected(uint32_t clientId) const {
    std::lock_guard lock(clientsMutex);
    return clients.contains(clientId);
}

size_t NetworkServer::GetClientCount() const {
    std::lock_guard lock(clientsMutex);
    return clients.size();
}

NetworkServer::ServerStats NetworkServer::GetStats() const {
    std::lock_guard lock(clientsMutex);
    ServerStats stats{};
    float totalRtt = 0.0f;
    size_t rttCount = 0;

    for (const auto& [id, client] : clients) {
        stats.totalBytesSent += client.bytesSent;
        stats.totalBytesRecv += client.bytesRecv;
        stats.totalPacketsSent += client.packetsSent;
        stats.totalPacketsRecv += client.packetsRecv;

        if (client.channel) {
            totalRtt += client.channel->GetRtt();
            rttCount++;
        }
    }

    if (rttCount > 0) {
        stats.averageRtt = totalRtt / static_cast<float>(rttCount);
    }

    return stats;
}

uint32_t NetworkServer::GetOrCreateClientId(const Endpoint& endpoint) {
    std::lock_guard lock(clientsMutex);

    std::string epKey = endpoint.ToString();
    auto it = endpointToClientId.find(epKey);
    if (it != endpointToClientId.end()) {
        return it->second;
    }

    if (clients.size() >= maxClients) {
        AddLog("[NetworkServer] Max clients reached ({})");
        return 0;
    }

    uint32_t clientId = nextClientId++;

    ClientConnection conn;
    conn.clientId = clientId;
    conn.endpoint = endpoint;
    conn.channel = std::make_unique<ReliableChannel>(&socket, endpoint);
    conn.lastRecvTime = std::chrono::steady_clock::now();

    clients[clientId] = std::move(conn);
    endpointToClientId[epKey] = clientId;

    return clientId;
}

void NetworkServer::RemoveClient(uint32_t clientId) {
    std::lock_guard lock(clientsMutex);
    auto it = clients.find(clientId);
    if (it != clients.end()) {
        endpointToClientId.erase(it->second.endpoint.ToString());
        clients.erase(it);
    }
}

void NetworkServer::CleanupTimedOutClients() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint32_t> toRemove;

    {
        std::lock_guard lock(clientsMutex);
        for (const auto& [id, client] : clients) {
            float elapsed = std::chrono::duration<float>(now - client.lastRecvTime).count();
            if (elapsed > clientTimeout) {
                toRemove.push_back(id);
            }
        }
    }

    for (uint32_t id : toRemove) {
        AddLog("[NetworkServer] Client {} timed out", id);
        DisconnectClient(id);
    }
}

} // namespace net
