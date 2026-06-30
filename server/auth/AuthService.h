// =============================================================================
// server/auth/AuthService.h — JWT + Argon2id Authentication (AP-45)
// Dependencies: libsodium (vcpkg)
// =============================================================================
// KORREKTUR (AP-45 Fix): Hartkodierte Credentials entfernt.
// AuthService nutzt jetzt IUserRepository für DB-Zugriff.
// Keine hartkodierten Test-Credentials mehr.
// =============================================================================
#pragma once
#include "IUserRepository.h"
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <expected>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace auth {

enum class AuthError {
    InvalidCredentials,
    AccountNotFound,
    AccountLocked,
    AccountBanned,
    RateLimited,
    TokenExpired,
    TokenInvalid,
    InternalError,
    RepositoryUnavailable
};

struct Token {
    std::string accessToken;
    std::string refreshToken;
    std::chrono::steady_clock::time_point accessExpiry;
    std::chrono::steady_clock::time_point refreshExpiry;
};

struct AuthConfig {
    // Argon2id parameters (OWASP recommended minimum)
    uint32_t argon2Iterations = 3;      // t_cost
    uint32_t argon2Memory = 65536;      // m_cost (64MB)
    uint32_t argon2Parallelism = 4;     // threads

    // JWT settings
    std::chrono::minutes accessTokenLifetime{15};
    std::chrono::days refreshTokenLifetime{7};

    // Rate limiting (in-memory; production: Redis)
    uint32_t maxAttemptsPerMinute = 5;
    std::chrono::minutes lockoutDuration{15};

    // Account lockout
    uint32_t maxFailedAttempts = 10;    // Before account-level lock
    std::chrono::minutes accountLockoutDuration{60};
};

// =============================================================================
// AuthService — Production-Ready Authentication
// =============================================================================
class AuthService {
    AuthConfig config;
    std::unique_ptr<IUserRepository> userRepo;  // Dependency Injection

    // Rate limiting state (in production: replace with Redis)
    struct RateLimitEntry {
        uint32_t attemptCount = 0;
        std::chrono::steady_clock::time_point firstAttempt;
        std::chrono::steady_clock::time_point lockedUntil;
        bool isLocked = false;
    };
    std::unordered_map<std::string, RateLimitEntry> rateLimitMap;
    std::mutex rateLimitMutex;

    // JWT signing key (in production: load from secure key storage / HSM)
    std::vector<uint8_t> jwtSecret;

public:
    // ===================================================================
    // Construction: Repository wird injected (SQLite, PostgreSQL, Mock)
    // ===================================================================
    explicit AuthService(std::unique_ptr<IUserRepository> repo,
                         const AuthConfig& cfg = AuthConfig{});
    ~AuthService() = default;

    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;

    // ===================================================================
    // Password Hashing (Argon2id via libsodium)
    // ===================================================================
    [[nodiscard]] std::vector<uint8_t> HashPassword(std::string_view password);
    [[nodiscard]] bool VerifyPassword(std::string_view password, std::span<const uint8_t> hash);

    // ===================================================================
    // Registration
    // ===================================================================
    [[nodiscard]] std::expected<void, AuthError> Register(
        std::string_view username,
        std::string_view password,
        std::string_view email = "");

    // ===================================================================
    // Authentication
    // ===================================================================
    [[nodiscard]] std::expected<Token, AuthError> Login(
        std::string_view username,
        std::string_view password,
        std::string_view clientIP);

    [[nodiscard]] std::expected<Token, AuthError> RefreshToken(std::string_view refreshToken);
    [[nodiscard]] bool VerifyToken(std::string_view accessToken);
    [[nodiscard]] std::optional<std::string> ExtractUsername(std::string_view accessToken);

    // ===================================================================
    // Account Management
    // ===================================================================
    [[nodiscard]] bool IsRateLimited(std::string_view clientIP);
    void RecordAttempt(std::string_view clientIP, bool success);

    // Force password change (admin operation)
    [[nodiscard]] std::expected<void, AuthError> ChangePassword(
        std::string_view username,
        std::string_view oldPassword,
        std::string_view newPassword);

private:
    [[nodiscard]] std::string GenerateJWT(std::string_view username, std::chrono::minutes lifetime);
    [[nodiscard]] bool ValidateJWT(std::string_view token, std::string& outUsername);
    void CleanupRateLimits();
};

} // namespace auth
