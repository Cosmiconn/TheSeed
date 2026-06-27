// =============================================================================
// network/UdpSocket.cpp — Cross-Platform UDP Implementation (AP-32)
// =============================================================================
#include "UdpSocket.h"
#include <cstring>

namespace net {

// =============================================================================
// SocketAddress
// =============================================================================

std::string SocketAddress::ToString() const {
    char addrStr[INET6_ADDRSTRLEN];
    if (IsIPv4()) {
        inet_ntop(AF_INET, &v4.sin_addr, addrStr, sizeof(addrStr));
        return std::format("{}:{}", addrStr, ntohs(v4.sin_port));
    } else {
        inet_ntop(AF_INET6, &v6.sin6_addr, addrStr, sizeof(addrStr));
        return std::format("[{}]:{}", addrStr, ntohs(v6.sin6_port));
    }
}

uint16_t SocketAddress::GetPort() const {
    if (IsIPv4()) return ntohs(v4.sin_port);
    return ntohs(v6.sin6_port);
}

// =============================================================================
// UdpSocket
// =============================================================================

UdpSocket::UdpSocket(UdpSocket&& other) noexcept 
    : fd(other.fd), isNonBlocking(other.isNonBlocking) {
    other.fd = INVALID_SOCKET_FD;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        Close();
        fd = other.fd;
        isNonBlocking = other.isNonBlocking;
        other.fd = INVALID_SOCKET_FD;
    }
    return *this;
}

bool UdpSocket::Bind(uint16_t port, bool ipv6) {
    Close();

    int family = ipv6 ? AF_INET6 : AF_INET;
    fd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET_FD) return false;

    if (ipv6) {
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        addr.sin6_addr = in6addr_any;

        // Allow IPv4-mapped IPv6 addresses
        int v6only = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, 
                   reinterpret_cast<const char*>(&v6only), sizeof(v6only));

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            Close();
            return false;
        }
    } else {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            Close();
            return false;
        }
    }

    return true;
}

bool UdpSocket::SetNonBlocking() {
    if (fd == INVALID_SOCKET_FD) return false;

#ifdef _WIN32
    u_long mode = 1;
    isNonBlocking = ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    isNonBlocking = fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
    return isNonBlocking;
}

bool UdpSocket::SetBroadcast(bool enabled) {
    if (fd == INVALID_SOCKET_FD) return false;
    int value = enabled ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, 
                      reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool UdpSocket::SetSendBufferSize(int size) {
    if (fd == INVALID_SOCKET_FD) return false;
    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, 
                      reinterpret_cast<const char*>(&size), sizeof(size)) == 0;
}

bool UdpSocket::SetRecvBufferSize(int size) {
    if (fd == INVALID_SOCKET_FD) return false;
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, 
                      reinterpret_cast<const char*>(&size), sizeof(size)) == 0;
}

std::expected<size_t, std::string> UdpSocket::SendTo(
    std::span<const uint8_t> data, 
    const SocketAddress& dest) {

    if (fd == INVALID_SOCKET_FD) 
        return std::unexpected("Socket not initialized");

    int sent = sendto(fd, 
                      reinterpret_cast<const char*>(data.data()), 
                      static_cast<int>(data.size()), 
                      0,
                      reinterpret_cast<const sockaddr*>(&dest.storage), 
                      dest.length);

    if (sent < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0; // Would block
        return std::unexpected(std::format("sendto failed: {}", err));
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return std::unexpected(std::format("sendto failed: {}", errno));
#endif
    }

    return static_cast<size_t>(sent);
}

std::expected<size_t, std::string> UdpSocket::RecvFrom(
    std::span<uint8_t> buffer,
    SocketAddress& outSender) {

    if (fd == INVALID_SOCKET_FD)
        return std::unexpected("Socket not initialized");

    outSender.length = sizeof(outSender.storage);
    int received = recvfrom(fd,
                            reinterpret_cast<char*>(buffer.data()),
                            static_cast<int>(buffer.size()),
                            0,
                            reinterpret_cast<sockaddr*>(&outSender.storage),
                            &outSender.length);

    if (received < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0; // No data available
        return std::unexpected(std::format("recvfrom failed: {}", err));
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return std::unexpected(std::format("recvfrom failed: {}", errno));
#endif
    }

    return static_cast<size_t>(received);
}

bool UdpSocket::HasData() const {
    if (fd == INVALID_SOCKET_FD) return false;

#ifdef _WIN32
    u_long available = 0;
    return ioctlsocket(fd, FIONREAD, &available) == 0 && available > 0;
#else
    int available = 0;
    return ioctl(fd, FIONREAD, &available) == 0 && available > 0;
#endif
}

void UdpSocket::Close() {
    if (fd != INVALID_SOCKET_FD) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        fd = INVALID_SOCKET_FD;
    }
}

bool UdpSocket::SetReuseAddr(bool enabled) {
    if (fd == INVALID_SOCKET_FD) return false;
    int value = enabled ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                      reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool UdpSocket::SetTTL(int ttl) {
    if (fd == INVALID_SOCKET_FD) return false;
#ifdef _WIN32
    return setsockopt(fd, IPPROTO_IP, IP_TTL,
                      reinterpret_cast<const char*>(&ttl), sizeof(ttl)) == 0;
#else
    return setsockopt(fd, IPPROTO_IP, IP_TTL,
                      &ttl, sizeof(ttl)) == 0;
#endif
}

// =============================================================================
// Network Init/Shutdown
// =============================================================================

bool NetworkInit() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true; // Nothing needed on Linux
#endif
}

void NetworkShutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace net
