// =============================================================================
// server/auth/AuthService.cpp — JWT + Argon2id Implementation (AP-45)
// =============================================================================
#include "AuthService.h"
#include "../../core/Log.h"

#include <sodium.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>
#include <base64.hpp>
#include <nlohmann/json.hpp>

namespace auth {

using json = nlohmann::json;

// =============================================================================
// Base64 Helpers
// =============================================================================
static std::string Base64Encode(std::span<const uint8_t> data) {
    return base64::encode_into<std::string>(data.begin(), data.end());
}

static std::vector<uint8_t> Base64Decode(std::string_view str) {
    return base64::decode_into<std::vector<uint8_t>>(str);
}

// =============================================================================
// JWT Helpers
// =============================================================================
static std::string CreateJWTHeader() {
    json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    std::string headerStr = header.dump();
    return Base64Encode(std::span(reinterpret_cast<const uint8_t*>(headerStr.data()), headerStr.size()));
}

static std::string SignHMAC256(std::string_view message, std::span<const uint8_t> key) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int resultLen = 0;

    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(),
         result, &resultLen);

    return Base64Encode(std::span(result, resultLen));
}

// =============================================================================
// AuthService Implementation
// =============================================================================
AuthService::AuthService(const AuthConfig& cfg) : config(cfg) {
    if (sodium_init() < 0) {
        AddLog("[Auth] CRITICAL: libsodium initialization failed!");
    }

    // Generate random JWT secret (in production: load from secure key storage)
    jwtSecret.resize(32);
    randombytes_buf(jwtSecret.data(), jwtSecret.size());

    AddLog("[Auth] AuthService initialized with Argon2id (t={}, m={}, p={})",
           config.argon2Iterations, config.argon2Memory, config.argon2Parallelism);
}

std::vector<uint8_t> AuthService::HashPassword(std::string_view password) {
    std::vector<uint8_t> hash(crypto_pwhash_STRBYTES);

    int result = crypto_pwhash_str(
        reinterpret_cast<char*>(hash.data()),
        password.data(),
        password.size(),
        config.argon2Iterations,
        config.argon2Memory,
        config.argon2Parallelism
    );

    if (result != 0) {
        AddLog("[Auth] Argon2id hashing failed (out of memory?)");
        return {};
    }

    return hash;
}

bool AuthService::VerifyPassword(std::string_view password, std::span<const uint8_t> hash) {
    if (hash.empty() || hash.size() < crypto_pwhash_STRBYTES) {
        return false;
    }

    int result = crypto_pwhash_str_verify(
        reinterpret_cast<const char*>(hash.data()),
        password.data(),
        password.size()
    );

    return result == 0;
}

std::expected<Token, AuthError> AuthService::Login(
    std::string_view username,
    std::string_view password,
    std::string_view clientIP) {

    // Rate limit check
    if (IsRateLimited(clientIP)) {
        return std::unexpected(AuthError::RateLimited);
    }

    // TODO: In production, query PostgreSQL for user record
    // For now, simulate with hardcoded test user
    if (username != "test" || password != "test") {
        RecordAttempt(clientIP, false);
        return std::unexpected(AuthError::InvalidCredentials);
    }

    RecordAttempt(clientIP, true);

    // Generate tokens
    Token token;
    token.accessToken = GenerateJWT(username, config.accessTokenLifetime);
    token.refreshToken = GenerateJWT(username + ":refresh", config.refreshTokenLifetime);
    token.accessExpiry = std::chrono::steady_clock::now() + config.accessTokenLifetime;
    token.refreshExpiry = std::chrono::steady_clock::now() + config.refreshTokenLifetime;

    AddLog("[Auth] User '{}' logged in from {}", username, clientIP);
    return token;
}

std::expected<Token, AuthError> AuthService::RefreshToken(std::string_view refreshToken) {
    std::string username;
    if (!ValidateJWT(refreshToken, username)) {
        return std::unexpected(AuthError::TokenInvalid);
    }

    // Verify it's a refresh token
    if (!username.ends_with(":refresh")) {
        return std::unexpected(AuthError::TokenInvalid);
    }

    username = username.substr(0, username.find(":refresh"));

    Token token;
    token.accessToken = GenerateJWT(username, config.accessTokenLifetime);
    token.refreshToken = GenerateJWT(username + ":refresh", config.refreshTokenLifetime);
    token.accessExpiry = std::chrono::steady_clock::now() + config.accessTokenLifetime;
    token.refreshExpiry = std::chrono::steady_clock::now() + config.refreshTokenLifetime;

    return token;
}

bool AuthService::VerifyToken(std::string_view accessToken) {
    std::string username;
    return ValidateJWT(accessToken, username);
}

std::optional<std::string> AuthService::ExtractUsername(std::string_view accessToken) {
    std::string username;
    if (ValidateJWT(accessToken, username)) {
        return username;
    }
    return std::nullopt;
}

bool AuthService::IsRateLimited(std::string_view clientIP) {
    std::lock_guard lock(rateLimitMutex);
    CleanupRateLimits();

    auto it = rateLimitMap.find(std::string(clientIP));
    if (it == rateLimitMap.end()) return false;

    if (it->second.isLocked) {
        auto now = std::chrono::steady_clock::now();
        if (now < it->second.lockedUntil) {
            return true;
        }
        it->second.isLocked = false;
        it->second.attemptCount = 0;
    }

    return it->second.attemptCount >= config.maxAttemptsPerMinute;
}

void AuthService::RecordAttempt(std::string_view clientIP, bool success) {
    std::lock_guard lock(rateLimitMutex);

    auto& entry = rateLimitMap[std::string(clientIP)];
    auto now = std::chrono::steady_clock::now();

    if (success) {
        entry.attemptCount = 0;
        entry.isLocked = false;
        return;
    }

    if (entry.attemptCount == 0) {
        entry.firstAttempt = now;
    }

    entry.attemptCount++;

    if (entry.attemptCount >= config.maxAttemptsPerMinute) {
        entry.isLocked = true;
        entry.lockedUntil = now + config.lockoutDuration;
        AddLog("[Auth] Rate limit triggered for IP {} (locked for {} min)",
               clientIP, config.lockoutDuration.count());
    }
}

std::string AuthService::GenerateJWT(std::string_view username, std::chrono::minutes lifetime) {
    std::string header = CreateJWTHeader();

    auto now = std::chrono::system_clock::now();
    auto expiry = now + lifetime;

    json payload = {
        {"sub", std::string(username)},
        {"iat", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()},
        {"exp", std::chrono::duration_cast<std::chrono::seconds>(expiry.time_since_epoch()).count()},
        {"jti", std::format("{}", randombytes_random())}
    };

    std::string payloadStr = payload.dump();
    std::string payloadB64 = Base64Encode(std::span(
        reinterpret_cast<const uint8_t*>(payloadStr.data()), payloadStr.size()));

    std::string toSign = header + "." + payloadB64;
    std::string signature = SignHMAC256(toSign, jwtSecret);

    return toSign + "." + signature;
}

bool AuthService::ValidateJWT(std::string_view token, std::string& outUsername) {
    // Split token
    size_t firstDot = token.find('.');
    size_t secondDot = token.find('.', firstDot + 1);

    if (firstDot == std::string_view::npos || secondDot == std::string_view::npos) {
        return false;
    }

    std::string_view headerB64 = token.substr(0, firstDot);
    std::string_view payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string_view signature = token.substr(secondDot + 1);

    // Verify signature
    std::string toSign = std::string(headerB64) + "." + std::string(payloadB64);
    std::string expectedSig = SignHMAC256(toSign, jwtSecret);

    if (!std::equal(signature.begin(), signature.end(), expectedSig.begin(), expectedSig.end())) {
        return false;
    }

    // Decode payload
    auto payloadBytes = Base64Decode(std::string(payloadB64));
    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());

    try {
        json payload = json::parse(payloadStr);

        // Check expiry
        if (payload.contains("exp")) {
            auto now = std::chrono::system_clock::now();
            auto exp = std::chrono::seconds(payload["exp"].get<int64_t>());
            if (now.time_since_epoch() > exp) {
                return false;
            }
        }

        outUsername = payload["sub"].get<std::string>();
        return true;
    } catch (...) {
        return false;
    }
}

void AuthService::CleanupRateLimits() {
    auto now = std::chrono::steady_clock::now();
    std::erase_if(rateLimitMap, [&now](const auto& pair) {
        const auto& entry = pair.second;
        // Remove entries older than 1 hour with no activity
        return !entry.isLocked && 
               std::chrono::duration_cast<std::chrono::hours>(now - entry.firstAttempt).count() > 1;
    });
}

} // namespace auth
