#pragma once
// =============================================================================
// network/UdpSocket.h — Cross-Platform UDP Socket (AP-32)
// Windows: IOCP-ready | Linux: epoll-ready
// =============================================================================
#include <cstdint>
#include <string_view>
#include <span>
#include <optional>
#include <expected>
#include <string>
#include <format>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketFd = SOCKET;
    constexpr SocketFd INVALID_SOCKET_FD = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    using SocketFd = int;
    constexpr SocketFd INVALID_SOCKET_FD = -1;
#endif

namespace net {

struct SocketAddress {
    union {
        sockaddr_in v4;
        sockaddr_in6 v6;
        sockaddr_storage storage;
    };
    socklen_t length = sizeof(sockaddr_storage);

    SocketAddress() = default;
    explicit SocketAddress(const sockaddr_in& addr) : v4(addr), length(sizeof(addr)) {}
    explicit SocketAddress(const sockaddr_in6& addr) : v6(addr), length(sizeof(addr)) {}

    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] uint16_t GetPort() const;
    [[nodiscard]] bool IsIPv4() const noexcept { return v4.sin_family == AF_INET; }
};

// UDP Socket with non-blocking I/O
class UdpSocket {
    SocketFd fd = INVALID_SOCKET_FD;
    bool isNonBlocking = false;

public:
    UdpSocket() = default;
    ~UdpSocket() { Close(); }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // Create and bind UDP socket
    [[nodiscard]] bool Bind(uint16_t port, bool ipv6 = false);

    // Enable non-blocking mode
    [[nodiscard]] bool SetNonBlocking();

    // Enable broadcast
    [[nodiscard]] bool SetBroadcast(bool enabled);

    // Set socket buffer sizes
    [[nodiscard]] bool SetSendBufferSize(int size);
    [[nodiscard]] bool SetRecvBufferSize(int size);

    // Send datagram
    [[nodiscard]] std::expected<size_t, std::string> SendTo(
        std::span<const uint8_t> data, 
        const SocketAddress& dest);

    // Receive datagram
    [[nodiscard]] std::expected<size_t, std::string> RecvFrom(
        std::span<uint8_t> buffer,
        SocketAddress& outSender);

    // Check if data is available (non-blocking)
    [[nodiscard]] bool HasData() const;

    void Close();
    [[nodiscard]] bool IsValid() const noexcept { return fd != INVALID_SOCKET_FD; }
    [[nodiscard]] SocketFd GetFd() const noexcept { return fd; }

    // Platform-specific socket options
    [[nodiscard]] bool SetReuseAddr(bool enabled);
    [[nodiscard]] bool SetTTL(int ttl);
};

// Network initialization/shutdown (WSAStartup / nothing on Linux)
[[nodiscard]] bool NetworkInit();
void NetworkShutdown();

} // namespace net
