#pragma once
// =============================================================================
// network/UdpSocket.h — Cross-Platform UDP Socket Abstraction (AP-32)
// =============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <span>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    using SOCKET = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

namespace net {

struct Endpoint {
    sockaddr_in6 addr{};
    socklen_t addrLen = sizeof(sockaddr_in6);

    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] uint16_t GetPort() const;
    [[nodiscard]] bool IsIPv4() const;

    static Endpoint FromIPv4(const std::string& ip, uint16_t port);
    static Endpoint FromIPv6(const std::string& ip, uint16_t port);
    static Endpoint Any(uint16_t port, bool ipv6 = true);
};

class UdpSocket {
    SOCKET fd = INVALID_SOCKET;
    bool nonBlocking = false;

public:
    UdpSocket() = default;
    ~UdpSocket() { Close(); }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Create(bool ipv6 = true);
    void Close();
    [[nodiscard]] bool IsValid() const { return fd != INVALID_SOCKET; }

    // ===================================================================
    // Configuration
    // ===================================================================
    [[nodiscard]] bool SetNonBlocking(bool enable);
    [[nodiscard]] bool SetReuseAddr(bool enable);
    [[nodiscard]] bool SetRecvBufferSize(int size);
    [[nodiscard]] bool SetSendBufferSize(int size);
    [[nodiscard]] bool Bind(const Endpoint& endpoint);

    // ===================================================================
    // I/O
    // ===================================================================
    [[nodiscard]] std::optional<int> SendTo(std::span<const uint8_t> data, const Endpoint& dest);
    [[nodiscard]] std::optional<int> RecvFrom(std::span<uint8_t> buffer, Endpoint& outSender);

    // Non-blocking peek: returns 0 if no data, >0 if data available
    [[nodiscard]] int Peek() const;

    [[nodiscard]] SOCKET NativeHandle() const { return fd; }
};

} // namespace net
