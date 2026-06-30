// =============================================================================
// network/network_UdpSocket.cpp — Cross-Platform UDP Implementation (AP-32)
// =============================================================================
#include "network_UdpSocket.h"
#include "../core/Log.h"

#include <cstring>

namespace net {

#ifdef _WIN32
int UdpSocket::wsaInitCount = 0;

bool UdpSocket::InitializeWinsock() {
    if (wsaInitCount == 0) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            AddLog("[Net] WSAStartup fehlgeschlagen: {}", result);
            return false;
        }
        AddLog("[Net] Winsock initialisiert");
    }
    wsaInitCount++;
    return true;
}

void UdpSocket::CleanupWinsock() {
    wsaInitCount--;
    if (wsaInitCount <= 0) {
        WSACleanup();
        wsaInitCount = 0;
        AddLog("[Net] Winsock bereinigt");
    }
}
#endif

// =============================================================================
// Konstruktor / Destruktor
// =============================================================================
UdpSocket::UdpSocket() {
    #ifdef _WIN32
    InitializeWinsock();
    #endif
}

UdpSocket::~UdpSocket() {
    Close();
    #ifdef _WIN32
    CleanupWinsock();
    #endif
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : socketHandle(other.socketHandle)
    , boundPort(other.boundPort)
    , isNonBlocking(other.isNonBlocking) {
    other.socketHandle = INVALID_SOCKET_VALUE;
    other.boundPort = 0;
    #ifdef _WIN32
    wsaInitCount++; // Übernehme Referenz
    #endif
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        Close();
        socketHandle = other.socketHandle;
        boundPort = other.boundPort;
        isNonBlocking = other.isNonBlocking;
        other.socketHandle = INVALID_SOCKET_VALUE;
        other.boundPort = 0;
        #ifdef _WIN32
        wsaInitCount++;
        #endif
    }
    return *this;
}

// =============================================================================
// Create
// =============================================================================
bool UdpSocket::Create(uint16_t port) {
    socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET_VALUE) {
        AddLog("[Net] socket() fehlgeschlagen");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketHandle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        AddLog("[Net] bind() fehlgeschlagen auf Port {}", port);
        Close();
        return false;
    }

    boundPort = port;
    AddLog("[Net] UDP Socket gebunden auf Port {}", port);
    return true;
}

// =============================================================================
// Close
// =============================================================================
void UdpSocket::Close() {
    if (socketHandle != INVALID_SOCKET_VALUE) {
        #ifdef _WIN32
        closesocket(socketHandle);
        #else
        close(socketHandle);
        #endif
        socketHandle = INVALID_SOCKET_VALUE;
        boundPort = 0;
    }
}

// =============================================================================
// ReceiveFrom
// =============================================================================
int UdpSocket::ReceiveFrom(std::vector<uint8_t>& buffer, std::string& senderIp, uint16_t& senderPort) {
    if (!IsValid()) return -1;

    sockaddr_in senderAddr{};
    #ifdef _WIN32
    int addrLen = sizeof(senderAddr);
    #else
    socklen_t addrLen = sizeof(senderAddr);
    #endif

    int received = recvfrom(socketHandle,
                            reinterpret_cast<char*>(buffer.data()),
                            static_cast<int>(buffer.size()),
                            0,
                            reinterpret_cast<sockaddr*>(&senderAddr),
                            &addrLen);

    if (received > 0) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        senderIp = ipStr;
        senderPort = ntohs(senderAddr.sin_port);
    } else if (received < 0) {
        #ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            AddLog("[Net] recvfrom() Fehler: {}", err);
        }
        #else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            AddLog("[Net] recvfrom() Fehler: {}", errno);
        }
        #endif
    }

    return received;
}

// =============================================================================
// SendTo
// =============================================================================
bool UdpSocket::SendTo(std::span<const uint8_t> data, std::string_view ip, uint16_t port) {
    if (!IsValid()) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.data(), &addr.sin_addr);

    int sent = sendto(socketHandle,
                      reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()),
                      0,
                      reinterpret_cast<sockaddr*>(&addr),
                      sizeof(addr));

    if (sent < 0) {
        #ifdef _WIN32
        AddLog("[Net] sendto() Fehler: {}", WSAGetLastError());
        #else
        AddLog("[Net] sendto() Fehler: {}", errno);
        #endif
        return false;
    }

    return static_cast<size_t>(sent) == data.size();
}

// =============================================================================
// SetNonBlocking
// =============================================================================
bool UdpSocket::SetNonBlocking() {
    if (!IsValid()) return false;

    #ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(socketHandle, FIONBIO, &mode) != 0) {
        AddLog("[Net] ioctlsocket(FIONBIO) fehlgeschlagen");
        return false;
    }
    #else
    int flags = fcntl(socketHandle, F_GETFL, 0);
    if (flags < 0) {
        AddLog("[Net] fcntl(F_GETFL) fehlgeschlagen");
        return false;
    }
    if (fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK) < 0) {
        AddLog("[Net] fcntl(F_SETFL) fehlgeschlagen");
        return false;
    }
    #endif

    isNonBlocking = true;
    AddLog("[Net] Socket auf non-blocking gesetzt");
    return true;
}

} // namespace net
