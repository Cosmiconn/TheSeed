#pragma once
// =============================================================================
// network/network_UdpSocket.h — Cross-Platform UDP Socket (AP-32)
// =============================================================================
// KORREKTUR: Vollständige Cross-Platform-Unterstützung für Windows und Linux.
// WSAStartup/WSACleanup korrekt gekapselt. Linux-Variante mit <sys/socket.h>.
// =============================================================================

#include <cstdint>
#include <string>
#include <span>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketType = SOCKET;
    constexpr SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    using SocketType = int;
    constexpr SocketType INVALID_SOCKET_VALUE = -1;
#endif

namespace net {

// =============================================================================
// UDP Socket Klasse
// =============================================================================
class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Create(uint16_t port);
    void Close();
    [[nodiscard]] bool IsValid() const { return socketHandle != INVALID_SOCKET_VALUE; }

    // ===================================================================
    // I/O
    // ===================================================================
    // Empfängt Daten. Gibt Anzahl empfangener Bytes zurück (0 = nichts, -1 = Fehler)
    int ReceiveFrom(std::vector<uint8_t>& buffer, std::string& senderIp, uint16_t& senderPort);

    // Sendet Daten an Ziel
    [[nodiscard]] bool SendTo(std::span<const uint8_t> data, std::string_view ip, uint16_t port);

    // Setzt Socket auf non-blocking
    [[nodiscard]] bool SetNonBlocking();

    // ===================================================================
    // Getters
    // ===================================================================
    [[nodiscard]] uint16_t GetBoundPort() const { return boundPort; }

private:
    SocketType socketHandle = INVALID_SOCKET_VALUE;
    uint16_t boundPort = 0;
    bool isNonBlocking = false;

    #ifdef _WIN32
    static int wsaInitCount;
    static bool InitializeWinsock();
    static void CleanupWinsock();
    #endif
};

} // namespace net
