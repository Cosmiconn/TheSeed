// =============================================================================
// server/auth/RedisRateLimiter.cpp — Redis Rate Limiting Implementation
// =============================================================================
#include "RedisRateLimiter.h"
#include "../../core/Log.h"

#include <hiredis/hiredis.h>
#include <cstdarg>
#include <cstring>

namespace auth {

// =============================================================================
// Construction / Destruction
// =============================================================================
RedisRateLimiter::RedisRateLimiter(std::string_view host, uint16_t port)
    : redisHost(host), redisPort(port) {}

RedisRateLimiter::~RedisRateLimiter() {
    Disconnect();
}

// =============================================================================
// Connection
// =============================================================================
std::expected<void, RedisError> RedisRateLimiter::Connect() {
    std::lock_guard lock(redisMutex);

    if (redis) {
        redisFree(redis);
    }

    redis = redisConnect(redisHost.c_str(), redisPort);
    if (!redis || redis->err) {
        AddLog("[Auth][Redis] Connection failed: {}", redis ? redis->errstr : "unknown");
        if (redis) {
            redisFree(redis);
            redis = nullptr;
        }
        return std::unexpected(RedisError::ConnectionFailed);
    }

    AddLog("[Auth][Redis] Connected to {}:{}", redisHost, redisPort);
    return {};
}

void RedisRateLimiter::Disconnect() {
    std::lock_guard lock(redisMutex);
    if (redis) {
        redisFree(redis);
        redis = nullptr;
    }
}

bool RedisRateLimiter::IsConnected() const {
    std::lock_guard lock(redisMutex);
    return redis != nullptr;
}

std::expected<void, RedisError> RedisRateLimiter::Ping() {
    auto reply = ExecuteCommand("PING");
    if (!reply) return std::unexpected(reply.error());
    FreeReply(*reply);
    return {};
}

// =============================================================================
// IP-based Rate Limiting (Sliding Window)
// =============================================================================
bool RedisRateLimiter::IsIpRateLimited(std::string_view clientIP) {
    if (!IsConnected()) return false; // Fallback: allow if Redis unavailable

    std::string key = MakeKey(std::format("ip:{}", clientIP));
    auto now = std::chrono::system_clock::now();
    auto windowStart = now - std::chrono::minutes(1);
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto windowTimestamp = std::chrono::duration_cast<std::chrono::seconds>(windowStart.time_since_epoch()).count();

    // Remove old entries (sliding window)
    auto remReply = ExecuteCommand("ZREMRANGEBYSCORE %s 0 %lld", key.c_str(), windowTimestamp);
    if (remReply) FreeReply(*remReply);

    // Count attempts in window
    auto countReply = ExecuteCommand("ZCARD %s", key.c_str());
    if (!countReply) return false;

    auto count = GetIntReply(*countReply);
    FreeReply(*countReply);

    if (!count) return false;
    return *count >= static_cast<int64_t>(config.maxAttemptsPerMinute);
}

void RedisRateLimiter::RecordIpAttempt(std::string_view clientIP, bool success) {
    if (!IsConnected()) return;

    std::string key = MakeKey(std::format("ip:{}", clientIP));
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    if (success) {
        // Reset on success
        auto delReply = ExecuteCommand("DEL %s", key.c_str());
        if (delReply) FreeReply(*delReply);
        return;
    }

    // Add attempt with timestamp (score)
    auto addReply = ExecuteCommand("ZADD %s %lld %lld", key.c_str(), timestamp, timestamp);
    if (addReply) FreeReply(*addReply);

    // Set expiry (2x window size)
    auto expireReply = ExecuteCommand("EXPIRE %s %d", key.c_str(), 120);
    if (expireReply) FreeReply(*expireReply);

    // Check if rate limit triggered
    if (IsIpRateLimited(clientIP)) {
        AddLog("[Auth][Redis] IP {} rate limited", clientIP);
    }
}

void RedisRateLimiter::ResetIpAttempts(std::string_view clientIP) {
    if (!IsConnected()) return;

    std::string key = MakeKey(std::format("ip:{}", clientIP));
    auto reply = ExecuteCommand("DEL %s", key.c_str());
    if (reply) FreeReply(*reply);
}

// =============================================================================
// Account-based Rate Limiting (Token Bucket)
// =============================================================================
bool RedisRateLimiter::IsAccountRateLimited(std::string_view username) {
    if (!IsConnected()) return false;

    std::string key = MakeKey(std::format("account:{}", username));

    // Check lockout
    auto lockReply = ExecuteCommand("GET %s:locked", key.c_str());
    if (lockReply && *lockReply) {
        auto lockExpiry = GetIntReply(*lockReply);
        FreeReply(*lockReply);
        if (lockExpiry) {
            auto now = std::chrono::system_clock::now();
            auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            if (*lockExpiry > nowSec) return true;
        }
    }

    // Check failed attempts
    auto attemptsReply = ExecuteCommand("GET %s:attempts", key.c_str());
    if (!attemptsReply || !*attemptsReply) {
        if (attemptsReply) FreeReply(*attemptsReply);
        return false;
    }

    auto attempts = GetIntReply(*attemptsReply);
    FreeReply(*attemptsReply);

    if (!attempts) return false;
    return *attempts >= static_cast<int64_t>(config.maxFailedAttempts);
}

void RedisRateLimiter::RecordAccountAttempt(std::string_view username, bool success) {
    if (!IsConnected()) return;

    std::string key = MakeKey(std::format("account:{}", username));

    if (success) {
        // Reset on success
        auto delReply = ExecuteCommand("DEL %s:attempts %s:locked", key.c_str(), key.c_str());
        if (delReply) FreeReply(*delReply);
        return;
    }

    // Increment failed attempts
    auto incrReply = ExecuteCommand("INCR %s:attempts", key.c_str());
    if (incrReply) {
        auto attempts = GetIntReply(*incrReply);
        FreeReply(*incrReply);

        // Set expiry on attempts
        auto expireReply = ExecuteCommand("EXPIRE %s:attempts %d", key.c_str(),
            static_cast<int>(config.accountLockoutDuration.count() * 60));
        if (expireReply) FreeReply(*expireReply);

        // Check if account should be locked
        if (attempts && *attempts >= static_cast<int64_t>(config.maxFailedAttempts)) {
            LockAccount(username, config.accountLockoutDuration);
            AddLog("[Auth][Redis] Account {} locked after {} failed attempts", username, *attempts);
        }
    }
}

void RedisRateLimiter::ResetAccountAttempts(std::string_view username) {
    if (!IsConnected()) return;

    std::string key = MakeKey(std::format("account:{}", username));
    auto reply = ExecuteCommand("DEL %s:attempts %s:locked", key.c_str(), key.c_str());
    if (reply) FreeReply(*reply);
}

uint32_t RedisRateLimiter::GetAccountFailedAttempts(std::string_view username) {
    if (!IsConnected()) return 0;

    std::string key = MakeKey(std::format("account:{}", username));
    auto reply = ExecuteCommand("GET %s:attempts", key.c_str());
    if (!reply || !*reply) {
        if (reply) FreeReply(*reply);
        return 0;
    }

    auto attempts = GetIntReply(*reply);
    FreeReply(*reply);
    return attempts ? static_cast<uint32_t>(*attempts) : 0;
}

// =============================================================================
// Lockout Management
// =============================================================================
void RedisRateLimiter::LockAccount(std::string_view username, std::chrono::minutes duration) {
    if (!IsConnected()) return;

    std::string key = MakeKey(std::format("account:{}", username));
    auto now = std::chrono::system_clock::now();
    auto expiry = now + duration;
    auto expirySec = std::chrono::duration_cast<std::chrono::seconds>(expiry.time_since_epoch()).count();

    auto reply = ExecuteCommand("SET %s:locked %lld EX %d", key.c_str(), expirySec,
        static_cast<int>(duration.count() * 60));
    if (reply) FreeReply(*reply);
}

void RedisRateLimiter::UnlockAccount(std::string_view username) {
    if (!IsConnected()) return;

    std::string key = MakeKey(std::format("account:{}", username));
    auto reply = ExecuteCommand("DEL %s:locked", key.c_str());
    if (reply) FreeReply(*reply);
}

bool RedisRateLimiter::IsAccountLocked(std::string_view username) {
    if (!IsConnected()) return false;

    std::string key = MakeKey(std::format("account:{}", username));
    auto reply = ExecuteCommand("GET %s:locked", key.c_str());
    if (!reply || !*reply) {
        if (reply) FreeReply(*reply);
        return false;
    }

    auto expiry = GetIntReply(*reply);
    FreeReply(*reply);

    if (!expiry) return false;

    auto now = std::chrono::system_clock::now();
    auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return *expiry > nowSec;
}

std::optional<std::chrono::system_clock::time_point> RedisRateLimiter::GetLockoutExpiry(
    std::string_view username) {

    if (!IsConnected()) return std::nullopt;

    std::string key = MakeKey(std::format("account:{}", username));
    auto reply = ExecuteCommand("GET %s:locked", key.c_str());
    if (!reply || !*reply) {
        if (reply) FreeReply(*reply);
        return std::nullopt;
    }

    auto expiry = GetIntReply(*reply);
    FreeReply(*reply);

    if (!expiry) return std::nullopt;
    return std::chrono::system_clock::time_point(std::chrono::seconds(*expiry));
}

// =============================================================================
// Global Stats
// =============================================================================
std::expected<uint64_t, RedisError> RedisRateLimiter::GetTotalAttempts(std::string_view key) {
    auto reply = ExecuteCommand("GET %s", MakeKey(key).c_str());
    if (!reply) return std::unexpected(reply.error());

    auto val = GetIntReply(*reply);
    FreeReply(*reply);

    if (!val) return std::unexpected(RedisError::InvalidResponse);
    return static_cast<uint64_t>(*val);
}

void RedisRateLimiter::ClearAllRateLimits() {
    if (!IsConnected()) return;

    auto reply = ExecuteCommand("EVAL "return redis.call('del', unpack(redis.call('keys', '%s*')))" 0",
        keyPrefix.c_str());
    if (reply) FreeReply(*reply);

    AddLog("[Auth][Redis] All rate limits cleared");
}

// =============================================================================
// Helpers
// =============================================================================
std::string RedisRateLimiter::MakeKey(std::string_view suffix) const {
    return std::format("{}{}", keyPrefix, suffix);
}

std::expected<redisReply*, RedisError> RedisRateLimiter::ExecuteCommand(const char* format, ...) {
    std::lock_guard lock(redisMutex);
    if (!redis) return std::unexpected(RedisError::ConnectionFailed);

    va_list args;
    va_start(args, format);
    redisReply* reply = static_cast<redisReply*>(redisvCommand(redis, format, args));
    va_end(args);

    if (!reply) {
        if (redis->err) {
            AddLog("[Auth][Redis] Command failed: {}", redis->errstr);
            return std::unexpected(RedisError::CommandFailed);
        }
        return std::unexpected(RedisError::InvalidResponse);
    }

    return reply;
}

void RedisRateLimiter::FreeReply(redisReply* reply) {
    if (reply) freeReplyObject(reply);
}

std::expected<int64_t, RedisError> RedisRateLimiter::GetIntReply(redisReply* reply) {
    if (!reply) return std::unexpected(RedisError::InvalidResponse);
    if (reply->type == REDIS_REPLY_INTEGER) return reply->integer;
    if (reply->type == REDIS_REPLY_STRING) return std::atoll(reply->str);
    if (reply->type == REDIS_REPLY_NIL) return 0;
    return std::unexpected(RedisError::InvalidResponse);
}

std::expected<std::string, RedisError> RedisRateLimiter::GetStringReply(redisReply* reply) {
    if (!reply) return std::unexpected(RedisError::InvalidResponse);
    if (reply->type == REDIS_REPLY_STRING) return std::string(reply->str, reply->len);
    if (reply->type == REDIS_REPLY_NIL) return std::string();
    return std::unexpected(RedisError::InvalidResponse);
}

} // namespace auth
