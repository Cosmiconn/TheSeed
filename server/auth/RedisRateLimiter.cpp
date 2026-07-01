// =============================================================================
// server/auth/RedisRateLimiter.cpp — Redis Rate Limiting (P3-FIX)
// =============================================================================
// KORREKTUR P3: Vollständige Implementierung mit hiredis.
// Wenn Redis nicht verfügbar: Stub mit Fehlermeldung.
// =============================================================================
#include "RedisRateLimiter.h"
#include "../../core/Log.h"

#ifdef REDIS_AVAILABLE
#include <hiredis/hiredis.h>

namespace auth {

// =============================================================================
// Konstruktor / Destruktor
// =============================================================================
RedisRateLimiter::RedisRateLimiter(std::string_view host, uint16_t port)
    : context(nullptr) {

    context = redisConnect(std::string(host).c_str(), port);

    if (context == nullptr || context->err) {
        if (context) {
            AddLog("[Auth][Redis] KRITISCH: Verbindung fehlgeschlagen: {}", context->errstr);
            redisFree(context);
            context = nullptr;
        } else {
            AddLog("[Auth][Redis] KRITISCH: Kann Redis-Kontext nicht allozieren");
        }
        return;
    }

    AddLog("[Auth][Redis] Verbindung hergestellt: {}:{}", host, port);
}

RedisRateLimiter::~RedisRateLimiter() {
    if (context) {
        redisFree(context);
    }
}

bool RedisRateLimiter::IsConnected() const {
    return context != nullptr && context->err == 0;
}

// =============================================================================
// Rate Limit prüfen (Sliding Window)
// =============================================================================
bool RedisRateLimiter::IsAllowed(std::string_view clientIP, uint32_t maxRequests, std::chrono::seconds window) {
    if (!IsConnected()) return true; // Erlaube alles wenn Redis nicht verfügbar

    std::string key = std::format("rate_limit:{}", clientIP);
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto windowMs = std::chrono::duration_cast<std::chrono::milliseconds>(window).count();
    auto oldestMs = nowMs - windowMs;

    // Redis Lua-Skript für atomare Sliding-Window-Prüfung
    const char* luaScript = R"(
        local key = KEYS[1]
        local now = tonumber(ARGV[1])
        local window = tonumber(ARGV[2])
        local maxRequests = tonumber(ARGV[3])
        local oldest = now - window

        -- Entferne alte Einträge
        redis.call('ZREMRANGEBYSCORE', key, 0, oldest)

        -- Zähle aktuelle Einträge
        local count = redis.call('ZCARD', key)

        if count < maxRequests then
            -- Erlaubt: Füge neuen Eintrag hinzu
            redis.call('ZADD', key, now, now)
            redis.call('PEXPIRE', key, window)
            return 1
        else
            -- Blockiert
            return 0
        end
    )";

    redisReply* reply = static_cast<redisReply*>(redisCommand(context,
        "EVAL %s 1 %s %lld %lld %u",
        luaScript,
        key.c_str(),
        nowMs,
        windowMs,
        maxRequests
    ));

    if (reply == nullptr) {
        AddLog("[Auth][Redis] Lua-Skript fehlgeschlagen: {}", context->errstr);
        return true; // Fail-open
    }

    bool allowed = false;
    if (reply->type == REDIS_REPLY_INTEGER) {
        allowed = (reply->integer == 1);
    }

    freeReplyObject(reply);
    return allowed;
}

// =============================================================================
// Anfrage protokollieren
// =============================================================================
void RedisRateLimiter::RecordRequest(std::string_view clientIP, std::chrono::seconds window) {
    if (!IsConnected()) return;

    std::string key = std::format("rate_limit:{}", clientIP);
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto windowMs = std::chrono::duration_cast<std::chrono::milliseconds>(window).count();

    redisReply* reply = static_cast<redisReply*>(redisCommand(context,
        "ZADD %s %lld %lld",
        key.c_str(),
        nowMs,
        nowMs
    ));

    if (reply) {
        freeReplyObject(reply);
    }

    // TTL setzen
    redisReply* expireReply = static_cast<redisReply*>(redisCommand(context,
        "PEXPIRE %s %lld",
        key.c_str(),
        windowMs
    ));

    if (expireReply) {
        freeReplyObject(expireReply);
    }
}

// =============================================================================
// Reset
// =============================================================================
void RedisRateLimiter::Reset(std::string_view clientIP) {
    if (!IsConnected()) return;

    std::string key = std::format("rate_limit:{}", clientIP);
    redisReply* reply = static_cast<redisReply*>(redisCommand(context,
        "DEL %s",
        key.c_str()
    ));

    if (reply) {
        freeReplyObject(reply);
    }

    AddLog("[Auth][Redis] Rate Limit zurückgesetzt für: {}", clientIP);
}

} // namespace auth

#else
// =============================================================================
// STUB: Redis nicht verfügbar
// =============================================================================
#include <chrono>

namespace auth {

RedisRateLimiter::RedisRateLimiter(std::string_view host, uint16_t port) {
    (void)host; (void)port;
    AddLog("[Auth] WARNUNG: RedisRateLimiter ist ein Stub (hiredis nicht verfügbar)");
}

RedisRateLimiter::~RedisRateLimiter() = default;

bool RedisRateLimiter::IsConnected() const { return false; }

bool RedisRateLimiter::IsAllowed(std::string_view clientIP, uint32_t maxRequests, std::chrono::seconds window) {
    (void)clientIP; (void)maxRequests; (void)window;
    return true; // Erlaube alles wenn Redis nicht verfügbar
}

void RedisRateLimiter::RecordRequest(std::string_view clientIP, std::chrono::seconds window) {
    (void)clientIP; (void)window;
}

void RedisRateLimiter::Reset(std::string_view clientIP) {
    (void)clientIP;
}

} // namespace auth
#endif
