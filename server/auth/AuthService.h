#pragma once
// =============================================================================
// server/auth/AuthService.h — JWT + Argon2id Authentication (P3-FIX)
// =============================================================================
// KORREKTUR P3: Alle fehlenden Includes ergänzt.
// libsodium und OpenSSL als optionale Abhängigkeiten.
// =============================================================================
#include "IUserRepository.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <expected>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <span>
#include <format>
#include <sstream>
#include <random>
#include <algorithm>

// Optionale Abhängigkeiten
#ifdef HAS_LIBSODIUM
#include <sodium.h>
#endif

#ifdef HAS_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#endif

namespace auth {

// =============================================================================
// AUTH-FEHLER
// =============================================================================
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

// =============================================================================
// TOKEN
// =============================================================================
struct Token {
    std::string accessToken;
    std::string refreshToken;
    std::chrono::steady_clock::time_point accessExpiry;
    std::chrono::steady_clock::time_point refreshExpiry;
};

// =============================================================================
// AUTH-KONFIGURATION
// =============================================================================
struct AuthConfig {
    // Argon2id Parameter (OWASP Minimum)
    uint32_t argon2Iterations = 3;      // t_cost
    uint32_t argon2Memory = 65536;      // m_cost (64MB)
    uint32_t argon2Parallelism = 4;     // threads

    // JWT-Einstellungen
    std::chrono::minutes accessTokenLifetime{15};
    std::chrono::days refreshTokenLifetime{7};

    // Rate Limiting (in-memory; Produktion: Redis)
    uint32_t maxAttemptsPerMinute = 5;
    std::chrono::minutes lockoutDuration{15};

    // Account-Lockout
    uint32_t maxFailedAttempts = 10;
    std::chrono::minutes accountLockoutDuration{60};
};

// =============================================================================
// AUTHSERVICE — Produktionsreife Authentifizierung
// =============================================================================
class AuthService {
    AuthConfig config;
    std::unique_ptr<IUserRepository> userRepo; // Dependency Injection

    // Rate Limiting Zustand (in Produktion: Redis ersetzen)
    struct RateLimitEntry {
        uint32_t attemptCount = 0;
        std::chrono::steady_clock::time_point firstAttempt;
        std::chrono::steady_clock::time_point lockedUntil;
        bool isLocked = false;
    };
    std::unordered_map<std::string, RateLimitEntry> rateLimitMap;
    std::mutex rateLimitMutex;

    // JWT Signing-Key (in Produktion: HSM / Vault)
    std::vector<uint8_t> jwtSecret;

public:
    // ===================================================================
    // Konstruktion: Repository wird injected (SQLite, PostgreSQL, Mock)
    // ===================================================================
    explicit AuthService(std::unique_ptr<IUserRepository> repo,
                         const AuthConfig& cfg = AuthConfig{});
    ~AuthService() = default;

    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;

    // ===================================================================
    // Passwort-Hashing (Argon2id via libsodium)
    // ===================================================================
    [[nodiscard]] std::vector<uint8_t> HashPassword(std::string_view password);
    [[nodiscard]] bool VerifyPassword(std::string_view password, std::span<const uint8_t> hash);

    // ===================================================================
    // Registrierung
    // ===================================================================
    [[nodiscard]] std::expected<void, AuthError> Register(
        std::string_view username,
        std::string_view password,
        std::string_view email = "");

    // ===================================================================
    // Authentifizierung
    // ===================================================================
    [[nodiscard]] std::expected<Token, AuthError> Login(
        std::string_view username,
        std::string_view password,
        std::string_view clientIP);

    [[nodiscard]] std::expected<Token, AuthError> RefreshToken(std::string_view refreshToken);
    [[nodiscard]] bool VerifyToken(std::string_view accessToken);
    [[nodiscard]] std::optional<std::string> ExtractUsername(std::string_view accessToken);

    // ===================================================================
    // Account-Management
    // ===================================================================
    [[nodiscard]] bool IsRateLimited(std::string_view clientIP);
    void RecordAttempt(std::string_view clientIP, bool success);

    // Passwort ändern (Admin-Operation)
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
