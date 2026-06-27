#pragma once
// =============================================================================
// client/Connection.h — Client Network Connection (STUB for V13.1)
// =============================================================================
#include <cstdint>
#include <string_view>
#include <span>
#include <vector>
#include "../core/ByteBuffer.h"

bool ClientConnect(std::string_view host, uint16_t port);
void ClientDisconnect();
void ClientSend(const ByteBuffer& buf);
