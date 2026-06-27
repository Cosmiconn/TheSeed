// =============================================================================
// client/Connection.cpp — Client Connection Implementation (STUB)
// =============================================================================
#include "Connection.h"
#include "../core/Log.h"

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

static SOCKET clientSocket = INVALID_SOCKET;

bool ClientConnect(std::string_view host, uint16_t port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) return false;

    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, host.data(), &sa.sin_addr);

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    u_long nb = 1;
    ioctlsocket(clientSocket, FIONBIO, &nb);

    AddLog("[Client] Connected to {}:{}", host, port);
    return true;
}

void ClientDisconnect() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    WSACleanup();
    AddLog("[Client] Disconnected.");
}

void ClientSend(const ByteBuffer& buf) {
    if (clientSocket == INVALID_SOCKET) return;
    uint8_t hdr[2];
    uint16_t len = static_cast<uint16_t>(buf.data.size());
    hdr[0] = static_cast<uint8_t>((len >> 8) & 0xFF);
    hdr[1] = static_cast<uint8_t>(len & 0xFF);
    send(clientSocket, reinterpret_cast<const char*>(hdr), 2, 0);
    send(clientSocket, reinterpret_cast<const char*>(buf.data.data()), static_cast<int>(buf.data.size()), 0);
}
