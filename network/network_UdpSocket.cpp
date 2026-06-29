// =============================================================================
// network/UdpSocket.cpp — Cross-Platform UDP Implementation (AP-32)
// =============================================================================
#include "network_UdpSocket.h"
#include "../core/Log.h"
#include <cstring>

namespace net {

// =============================================================================
// Endpoint Helpers
// =============================================================================
std::string Endpoint::ToString() const {
    char buf[INET6_ADDRSTRLEN];
    if (IsIPv4()) {
        auto* sin = reinterpret_cast<const sockaddr_in*>(&addr);
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        return std::format("{}:{}", buf, ntohs(sin->sin_port));
    } else {
        inet_ntop(AF_INET6, &addr.sin6_addr, buf, sizeof(buf));
        return std::format("[{}]:{}", buf, ntohs(addr.sin6_port));
    }
}

uint16_t Endpoint::GetPort() const {
    if (IsIPv4()) {
        return ntohs(reinterpret_cast<const sockaddr_in*>(&addr)->sin_port);
    }
    return ntohs(addr.sin6_port);
}

bool Endpoint::IsIPv4() const {
    return addr.sin6_family == AF_INET;
}

Endpoint Endpoint::FromIPv4(const std::string& ip, uint16_t port) {
    Endpoint ep;
    auto* sin = reinterpret_cast<sockaddr_in*>(&ep.addr);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &sin->sin_addr);
    ep.addrLen = sizeof(sockaddr_in);
    return ep;
}

Endpoint Endpoint::FromIPv6(const std::string& ip, uint16_t port) {
    Endpoint ep;
    ep.addr.sin6_family = AF_INET6;
    ep.addr.sin6_port = htons(port);
    inet_pton(AF_INET6, ip.c_str(), &ep.addr.sin6_addr);
    ep.addrLen = sizeof(sockaddr_in6);
    return ep;
}

Endpoint Endpoint::Any(uint16_t port, bool ipv6) {
    if (ipv6) {
        Endpoint ep;
        ep.addr.sin6_family = AF_INET6;
        ep.addr.sin6_port = htons(port);
        ep.addr.sin6_addr = in6addr_any;
        ep.addrLen = sizeof(sockaddr_in6);
        return ep;
    }
    return FromIPv4("0.0.0.0", port);
}

// =============================================================================
// UdpSocket
// =============================================================================
UdpSocket::UdpSocket(UdpSocket&& other) noexcept : fd(other.fd), nonBlocking(other.nonBlocking) {
    other.fd = INVALID_SOCKET;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        Close();
        fd = other.fd;
        nonBlocking = other.nonBlocking;
        other.fd = INVALID_SOCKET;
    }
    return *this;
}

bool UdpSocket::Create(bool ipv6) {
    Close();
    fd = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) {
        AddLog("[UdpSocket] socket() failed: {}", WSAGetLastError());
        return false;
    }

    // Dual-stack: IPv6 socket can also accept IPv4 (if supported)
    if (ipv6) {
        int no = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no));
    }

    return true;
}

void UdpSocket::Close() {
    if (fd != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        fd = INVALID_SOCKET;
    }
}

bool UdpSocket::SetNonBlocking(bool enable) {
    nonBlocking = enable;
#ifdef _WIN32
    u_long mode = enable ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(fd, F_SETFL, flags) != -1;
#endif
}

bool UdpSocket::SetReuseAddr(bool enable) {
    int val = enable ? 1 : 0;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&val, sizeof(val)) == 0;
}

bool UdpSocket::SetRecvBufferSize(int size) {
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size)) == 0;
}

bool UdpSocket::SetSendBufferSize(int size) {
    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size)) == 0;
}

bool UdpSocket::Bind(const Endpoint& endpoint) {
    const sockaddr* sa = reinterpret_cast<const sockaddr*>(&endpoint.addr);
    return bind(fd, sa, endpoint.addrLen) == 0;
}

std::optional<int> UdpSocket::SendTo(std::span<const uint8_t> data, const Endpoint& dest) {
    int sent = sendto(fd, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 
                      0, reinterpret_cast<const sockaddr*>(&dest.addr), dest.addrLen);
    if (sent == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
        return std::nullopt;
    }
    return sent;
}

std::optional<int> UdpSocket::RecvFrom(std::span<uint8_t> buffer, Endpoint& outSender) {
    outSender.addrLen = sizeof(outSender.addr);
    int recvd = recvfrom(fd, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()),
                         0, reinterpret_cast<sockaddr*>(&outSender.addr), &outSender.addrLen);
    if (recvd == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
        return std::nullopt;
    }
    return recvd;
}

int UdpSocket::Peek() const {
    char buf[1];
    sockaddr_storage from{};
    socklen_t fromLen = sizeof(from);
    int recvd = recvfrom(fd, buf, 1, MSG_PEEK, reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (recvd == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        return (err == WSAEWOULDBLOCK) ? 0 : -1;
#else
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
#endif
    }
    return recvd;
}

} // namespace net
