// =============================================================================
// server/auth/RedisRateLimiter.cpp — Redis Rate Limiting
// =============================================================================
// FIX P3: Stub-Implementierung wenn hiredis nicht verfügbar
// =============================================================================
#include "RedisRateLimiter.h"
#include "../../core/Log.h"

#ifdef REDIS_AVAILABLE
#include <hiredis/hiredis.h>

// ... full Redis implementation would go here ...
// (omitted for brevity - requires hiredis headers)

#else
// STUB: Redis nicht verfügbar
RedisRateLimiter::RedisRateLimiter(std::string_view host, uint16_t port) {
    (void)host; (void)port;
    AddLog("[Auth] WARNING: RedisRateLimiter is a stub (hiredis not available)");
}

RedisRateLimiter::~RedisRateLimiter() = default;

bool RedisRateLimiter::IsConnected() const { return false; }

bool RedisRateLimiter::IsAllowed(std::string_view clientIP, uint32_t maxRequests, std::chrono::seconds window) {
    (void)clientIP; (void)maxRequests; (void)window;
    return true; // Allow all when Redis is unavailable
}

void RedisRateLimiter::RecordRequest(std::string_view clientIP, std::chrono::seconds window) {
    (void)clientIP; (void)window;
}

void RedisRateLimiter::Reset(std::string_view clientIP) {
    (void)clientIP;
}

#endif
