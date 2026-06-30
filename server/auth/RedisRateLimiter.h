// =============================================================================
// server/auth/RedisRateLimiter.h — Redis-basiertes Rate Limiting (AP-45)
// =============================================================================
// KORREKTUR: Verteiltes Rate Limiting via Redis (hiredis).
// Ersetzt in-memory Rate Limiting für Multi-Server-Setups.
// Unterstützt Sliding Window + Token Bucket Algorithmen.
// =============================================================================
#pragma once
#include <string>
#include <string_view>
#include <chrono>
#include <expected>
#include <optional>

// Forward declaration (hiredis)
struct redisContext;
struct redisReply;

namespace auth {

enum class RedisError {
    ConnectionFailed,
    CommandFailed,
    Timeout,
    InvalidResponse
};

// =============================================================================
// Rate Limit Config
// =============================================================================
struct RateLimitConfig {
    uint32_t maxAttemptsPerMinute = 5;
    std::chrono::minutes lockoutDuration{15};
    uint32_t maxFailedAttempts = 10;
    std::chrono::minutes accountLockoutDuration{60};
    bool useSlidingWindow = true;  // true = sliding window, false = fixed window
};

// =============================================================================
// RedisRateLimiter
// =============================================================================
class RedisRateLimiter {
    redisContext* redis = nullptr;
    std::string redisHost;
    uint16_t redisPort = 6379;
    std::string keyPrefix = "ratelimit:";

    RateLimitConfig config;
    mutable std::mutex redisMutex;

public:
    explicit RedisRateLimiter(std::string_view host = "localhost", uint16_t port = 6379);
    ~RedisRateLimiter();

    RedisRateLimiter(const RedisRateLimiter&) = delete;
    RedisRateLimiter& operator=(const RedisRateLimiter&) = delete;

    // ===================================================================
    // Connection
    // ===================================================================
    [[nodiscard]] std::expected<void, RedisError> Connect();
    void Disconnect();
    [[nodiscard]] bool IsConnected() const;
    [[nodiscard]] std::expected<void, RedisError> Ping();

    // ===================================================================
    // IP-based Rate Limiting (Sliding Window)
    // ===================================================================
    [[nodiscard]] bool IsIpRateLimited(std::string_view clientIP);
    void RecordIpAttempt(std::string_view clientIP, bool success);
    void ResetIpAttempts(std::string_view clientIP);

    // ===================================================================
    // Account-based Rate Limiting (Token Bucket)
    // ===================================================================
    [[nodiscard]] bool IsAccountRateLimited(std::string_view username);
    void RecordAccountAttempt(std::string_view username, bool success);
    void ResetAccountAttempts(std::string_view username);
    [[nodiscard]] uint32_t GetAccountFailedAttempts(std::string_view username);

    // ===================================================================
    // Lockout Management
    // ===================================================================
    void LockAccount(std::string_view username, std::chrono::minutes duration);
    void UnlockAccount(std::string_view username);
    [[nodiscard]] bool IsAccountLocked(std::string_view username);
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> GetLockoutExpiry(std::string_view username);

    // ===================================================================
    // Global Stats
    // ===================================================================
    [[nodiscard]] std::expected<uint64_t, RedisError> GetTotalAttempts(std::string_view key);
    void ClearAllRateLimits();

private:
    [[nodiscard]] std::string MakeKey(std::string_view suffix) const;
    [[nodiscard]] std::expected<redisReply*, RedisError> ExecuteCommand(const char* format, ...);
    void FreeReply(redisReply* reply);
    [[nodiscard]] std::expected<int64_t, RedisError> GetIntReply(redisReply* reply);
    [[nodiscard]] std::expected<std::string, RedisError> GetStringReply(redisReply* reply);
};

} // namespace auth
