// =============================================================================
// server/auth/AuthService.cpp — JWT + Argon2id Implementation (P3-FIX)
// =============================================================================
// KORREKTUR P3:
// • Alle fehlenden Includes ergänzt (<nlohmann/json.hpp>, <openssl/hmac.h>)
// • Fallback-Hashing ohne libsodium (PBKDF2-ähnlich)
// • Argon2IdVerify korrigiert (Rückgabetyp bool statt std::string)
// • libsodium-Initialisierung mit Fehlerbehandlung
// =============================================================================
#include "AuthService.h"
#include "../../core/Log.h"

#include <nlohmann/json.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace auth {

using json = nlohmann::json;

// =============================================================================
// FALLBACK HASHING (wenn libsodium nicht verfügbar)
// =============================================================================
#ifndef HAS_LIBSODIUM
static std::string GenerateSalt() {
    static thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
    );
    std::uniform_int_distribution<uint64_t> dist(0, std::numeric_limits<uint64_t>::max());

    std::ostringstream oss;
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng);
    }
    return oss.str(); // 64 Hex-Zeichen = 32 Bytes
}

static std::string HashWithSalt(std::string_view password, std::string_view salt, int iterations) {
    std::string combined = std::string(password) + std::string(salt);
    std::size_t hash = std::hash<std::string>{}(combined);

    for (int i = 0; i < iterations; ++i) {
        std::string roundInput = std::to_string(hash) + std::string(salt);
        hash = std::hash<std::string>{}(roundInput);
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

static bool Argon2IdVerifyFallback(std::string_view password, std::string_view hashString) {
    // Parse $seed$ format: $seed$v=1$iter=<salt>$<hash>
    if (!hashString.starts_with("$seed$")) return false;

    std::string_view remaining = hashString;
    remaining.remove_prefix(6); // Skip "$seed$"

    if (!remaining.starts_with("v=1$iter=")) return false;
    remaining.remove_prefix(9); // Skip "v=1$iter="

    size_t dollarPos = remaining.find('$');
    if (dollarPos == std::string_view::npos) return false;
    int iterations = std::stoi(std::string(remaining.substr(0, dollarPos)));
    remaining.remove_prefix(dollarPos + 1);

    dollarPos = remaining.find('$');
    if (dollarPos == std::string_view::npos) return false;
    std::string_view salt = remaining.substr(0, dollarPos);
    remaining.remove_prefix(dollarPos + 1);

    std::string_view storedHash = remaining;
    std::string computedHash = HashWithSalt(password, salt, iterations);

    // Konstante Zeit-Vergleich (Timing-Attack-Schutz)
    if (computedHash.size() != storedHash.size()) return false;
    bool match = true;
    for (size_t i = 0; i < computedHash.size(); ++i) {
        match &= (computedHash[i] == storedHash[i]);
    }
    return match;
}
#endif

// =============================================================================
// BASE64 HILFSFUNKTIONEN (RFC 4648)
// =============================================================================
static constexpr std::string_view BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(std::span<const uint8_t> data) {
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t octet_a = i < data.size() ? data[i] : 0;
        uint32_t octet_b = i + 1 < data.size() ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < data.size() ? data[i + 2] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        encoded.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        encoded.push_back(i + 1 < data.size() ? BASE64_CHARS[(triple >> 6) & 0x3F] : '=');
        encoded.push_back(i + 2 < data.size() ? BASE64_CHARS[triple & 0x3F] : '=');
    }
    return encoded;
}

static std::vector<uint8_t> Base64Decode(std::string_view str) {
    auto findChar = [](char c) -> uint8_t {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return 0;
    };

    std::vector<uint8_t> decoded;
    decoded.reserve((str.size() / 4) * 3);

    for (size_t i = 0; i < str.size(); i += 4) {
        uint32_t sextet_a = str[i] != '=' ? findChar(str[i]) : 0;
        uint32_t sextet_b = str[i + 1] != '=' ? findChar(str[i + 1]) : 0;
        uint32_t sextet_c = str[i + 2] != '=' ? findChar(str[i + 2]) : 0;
        uint32_t sextet_d = str[i + 3] != '=' ? findChar(str[i + 3]) : 0;

        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;

        if (str[i + 2] != '=') decoded.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
        if (str[i + 2] != '=') decoded.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
        if (str[i + 3] != '=') decoded.push_back(static_cast<uint8_t>(triple & 0xFF));
    }
    return decoded;
}

// =============================================================================
// JWT HILFSFUNKTIONEN
// =============================================================================
static std::string CreateJWTHeader() {
    json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    std::string headerStr = header.dump();
    return Base64Encode(std::span(
        reinterpret_cast<const uint8_t*>(headerStr.data()), headerStr.size()));
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
// AUTHSERVICE IMPLEMENTATION
// =============================================================================
AuthService::AuthService(std::unique_ptr<IUserRepository> repo, const AuthConfig& cfg)
    : config(cfg), userRepo(std::move(repo)) {

    if (!userRepo || !userRepo->IsHealthy()) {
        AddLog("[Auth] KRITISCH: UserRepository nicht verfügbar!");
    }

#ifdef HAS_LIBSODIUM
    if (sodium_init() < 0) {
        AddLog("[Auth] KRITISCH: libsodium Initialisierung fehlgeschlagen!");
    } else {
        AddLog("[Auth] libsodium initialisiert.");
    }

    // Zufälliger JWT-Secret (32 Bytes)
    jwtSecret.resize(32);
    randombytes_buf(jwtSecret.data(), jwtSecret.size());
#else
    AddLog("[Auth] WARNUNG: libsodium nicht verfügbar, verwende Fallback-Hashing.");

    // Fallback: Zufälliger JWT-Secret ohne libsodium
    jwtSecret.resize(32);
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (auto& byte : jwtSecret) {
        byte = dist(rng);
    }
#endif

    AddLog("[Auth] AuthService initialisiert (Argon2id: t={}, m={}, p={})",
           config.argon2Iterations, config.argon2Memory, config.argon2Parallelism);
}

// =============================================================================
// PASSWORT-HASHING (Argon2id)
// =============================================================================
std::vector<uint8_t> AuthService::HashPassword(std::string_view password) {
#ifdef HAS_LIBSODIUM
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
        AddLog("[Auth] Argon2id Hashing fehlgeschlagen (Speicher?)");
        return {};
    }

    return hash;
#else
    // FIX P3: Fallback ohne libsodium - PBKDF2-ähnlich
    AddLog("[Auth] WARNUNG: libsodium nicht verfügbar, verwende Fallback-Hashing");
    std::string salt = GenerateSalt();
    constexpr int ITERATIONS = 100000;
    std::string hashStr = HashWithSalt(password, salt, ITERATIONS);

    // Format: $seed$v=1$iter=<iter>$<salt>$<hash>
    std::string formatted = std::format("$seed$v=1$iter={}${}${}", ITERATIONS, salt, hashStr);
    std::vector<uint8_t> result;
    result.insert(result.end(), formatted.begin(), formatted.end());
    return result;
#endif
}

bool AuthService::VerifyPassword(std::string_view password, std::span<const uint8_t> hash) {
#ifdef HAS_LIBSODIUM
    if (hash.empty()) {
        return false;
    }

    int result = crypto_pwhash_str_verify(
        reinterpret_cast<const char*>(hash.data()),
        password.data(),
        password.size()
    );

    return result == 0;
#else
    // FIX P3: Fallback - parse $seed$ format and verify
    AddLog("[Auth] WARNUNG: libsodium nicht verfügbar, verwende Fallback-Verifikation");
    std::string hashStr(hash.begin(), hash.end());
    return Argon2IdVerifyFallback(password, hashStr);
#endif
}

// =============================================================================
// REGISTRIERUNG
// =============================================================================
std::expected<void, AuthError> AuthService::Register(
    std::string_view username,
    std::string_view password,
    std::string_view email) {

    if (!userRepo || !userRepo->IsHealthy()) {
        return std::unexpected(AuthError::RepositoryUnavailable);
    }

    // Eingabe validieren
    if (username.empty() || username.length() < 3 || username.length() > 32) {
        return std::unexpected(AuthError::InvalidCredentials);
    }
    if (password.empty() || password.length() < 8) {
        return std::unexpected(AuthError::InvalidCredentials);
    }

    // Prüfe ob Benutzer bereits existiert
    auto exists = userRepo->UserExists(username);
    if (!exists) {
        return std::unexpected(AuthError::InternalError);
    }
    if (*exists) {
        return std::unexpected(AuthError::InvalidCredentials); // Existenz nicht preisgeben
    }

    // Passwort hashen
    auto hash = HashPassword(password);
    if (hash.empty()) {
        return std::unexpected(AuthError::InternalError);
    }

    // Benutzer erstellen
    UserRecord record;
    record.username = std::string(username);
    record.passwordHash = std::move(hash);
    record.email = std::string(email);
    record.createdAt = std::chrono::system_clock::now();

    auto result = userRepo->CreateUser(record);
    if (!result) {
        if (result.error() == RepositoryError::DuplicateUser) {
            return std::unexpected(AuthError::InvalidCredentials);
        }
        return std::unexpected(AuthError::InternalError);
    }

    AddLog("[Auth] Neuer Benutzer registriert: {}", username);
    return {};
}

// =============================================================================
// LOGIN — KORREKTUR: Keine hartkodierten Credentials!
// =============================================================================
std::expected<Token, AuthError> AuthService::Login(
    std::string_view username,
    std::string_view password,
    std::string_view clientIP) {

    if (!userRepo || !userRepo->IsHealthy()) {
        return std::unexpected(AuthError::RepositoryUnavailable);
    }

    // Rate Limit prüfen (IP-basiert)
    if (IsRateLimited(clientIP)) {
        return std::unexpected(AuthError::RateLimited);
    }

    // Benutzer aus Datenbank laden
    auto userResult = userRepo->FindByUsername(username);
    if (!userResult) {
        if (userResult.error() == RepositoryError::UserNotFound) {
            RecordAttempt(clientIP, false);
            return std::unexpected(AuthError::InvalidCredentials);
        }
        return std::unexpected(AuthError::InternalError);
    }

    const auto& user = *userResult;

    // Prüfe ob Account gesperrt
    if (user.isBanned) {
        return std::unexpected(AuthError::AccountBanned);
    }

    // Account-Level Lockout prüfen
    if (user.failedLoginAttempts >= config.maxFailedAttempts) {
        auto now = std::chrono::system_clock::now();
        if (now < user.lockedUntil) {
            return std::unexpected(AuthError::AccountLocked);
        }
        // Lock abgelaufen, Versuche zurücksetzen
        userRepo->UpdateLoginAttempts(username, 0, std::chrono::system_clock::time_point{});
    }

    // Passwort mit Argon2id verifizieren
    bool passwordValid = VerifyPassword(password, user.passwordHash);

    if (!passwordValid) {
        RecordAttempt(clientIP, false);

        // Fehlversuche in DB inkrementieren
        uint32_t newAttempts = user.failedLoginAttempts + 1;
        auto lockUntil = newAttempts >= config.maxFailedAttempts
            ? std::chrono::system_clock::now() + config.accountLockoutDuration
            : std::chrono::system_clock::time_point{};

        userRepo->UpdateLoginAttempts(username, newAttempts, lockUntil);

        return std::unexpected(AuthError::InvalidCredentials);
    }

    // Erfolg: Fehlversuche zurücksetzen und letzten Login aktualisieren
    RecordAttempt(clientIP, true);
    userRepo->UpdateLastLogin(username, clientIP);
    userRepo->UpdateLoginAttempts(username, 0, std::chrono::system_clock::time_point{});

    // Tokens generieren
    Token token;
    token.accessToken = GenerateJWT(username, config.accessTokenLifetime);
    token.refreshToken = GenerateJWT(std::string(username) + ":refresh", config.refreshTokenLifetime);
    token.accessExpiry = std::chrono::steady_clock::now() + config.accessTokenLifetime;
    token.refreshExpiry = std::chrono::steady_clock::now() + config.refreshTokenLifetime;

    AddLog("[Auth] Benutzer '{}' eingeloggt von {}", username, clientIP);
    return token;
}

// =============================================================================
// TOKEN ERNEUERN
// =============================================================================
std::expected<Token, AuthError> AuthService::RefreshToken(std::string_view refreshToken) {
    std::string username;
    if (!ValidateJWT(refreshToken, username)) {
        return std::unexpected(AuthError::TokenInvalid);
    }

    // Prüfe ob es ein Refresh-Token ist
    if (!username.ends_with(":refresh")) {
        return std::unexpected(AuthError::TokenInvalid);
    }

    username = username.substr(0, username.find(":refresh"));

    // Prüfe ob Benutzer noch existiert und aktiv ist
    if (!userRepo || !userRepo->IsHealthy()) {
        return std::unexpected(AuthError::RepositoryUnavailable);
    }

    auto userResult = userRepo->FindByUsername(username);
    if (!userResult || userResult->isBanned) {
        return std::unexpected(AuthError::TokenInvalid);
    }

    Token token;
    token.accessToken = GenerateJWT(username, config.accessTokenLifetime);
    token.refreshToken = GenerateJWT(std::string(username) + ":refresh", config.refreshTokenLifetime);
    token.accessExpiry = std::chrono::steady_clock::now() + config.accessTokenLifetime;
    token.refreshExpiry = std::chrono::steady_clock::now() + config.refreshTokenLifetime;

    return token;
}

// =============================================================================
// TOKEN VERIFIZIERUNG
// =============================================================================
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

// =============================================================================
// PASSWORT ÄNDERN
// =============================================================================
std::expected<void, AuthError> AuthService::ChangePassword(
    std::string_view username,
    std::string_view oldPassword,
    std::string_view newPassword) {

    if (!userRepo || !userRepo->IsHealthy()) {
        return std::unexpected(AuthError::RepositoryUnavailable);
    }

    if (newPassword.length() < 8) {
        return std::unexpected(AuthError::InvalidCredentials);
    }

    auto userResult = userRepo->FindByUsername(username);
    if (!userResult) {
        return std::unexpected(AuthError::AccountNotFound);
    }

    // Altes Passwort verifizieren
    if (!VerifyPassword(oldPassword, userResult->passwordHash)) {
        return std::unexpected(AuthError::InvalidCredentials);
    }

    // Neues Passwort hashen
    auto newHash = HashPassword(newPassword);
    if (newHash.empty()) {
        return std::unexpected(AuthError::InternalError);
    }

    auto result = userRepo->UpdatePasswordHash(username, newHash);
    if (!result) {
        return std::unexpected(AuthError::InternalError);
    }

    AddLog("[Auth] Passwort geändert für: {}", username);
    return {};
}

// =============================================================================
// RATE LIMITING (IP-basiert, in-memory)
// =============================================================================
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
        AddLog("[Auth] Rate Limit ausgelöst für IP {} (gesperrt für {} min)",
               clientIP, config.lockoutDuration.count());
    }
}

// =============================================================================
// JWT GENERIERUNG & VALIDIERUNG
// =============================================================================
std::string AuthService::GenerateJWT(std::string_view username, std::chrono::minutes lifetime) {
    std::string header = CreateJWTHeader();

    auto now = std::chrono::system_clock::now();
    auto expiry = now + lifetime;

    json payload = {
        {"sub", std::string(username)},
        {"iat", std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()},
        {"exp", std::chrono::duration_cast<std::chrono::seconds>(expiry.time_since_epoch()).count()},
        {"jti", std::format("{}", std::random_device{}())}
    };

    std::string payloadStr = payload.dump();
    std::string payloadB64 = Base64Encode(std::span(
        reinterpret_cast<const uint8_t*>(payloadStr.data()), payloadStr.size()));

    std::string toSign = header + "." + payloadB64;
    std::string signature = SignHMAC256(toSign, jwtSecret);

    return toSign + "." + signature;
}

bool AuthService::ValidateJWT(std::string_view token, std::string& outUsername) {
    // Token splitten
    size_t firstDot = token.find('.');
    size_t secondDot = token.find('.', firstDot + 1);

    if (firstDot == std::string_view::npos || secondDot == std::string_view::npos) {
        return false;
    }

    std::string_view headerB64 = token.substr(0, firstDot);
    std::string_view payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string_view signature = token.substr(secondDot + 1);

    // Signatur verifizieren
    std::string toSign = std::string(headerB64) + "." + std::string(payloadB64);
    std::string expectedSig = SignHMAC256(toSign, jwtSecret);

    if (!std::equal(signature.begin(), signature.end(), expectedSig.begin(), expectedSig.end())) {
        return false;
    }

    // Payload dekodieren
    auto payloadBytes = Base64Decode(std::string(payloadB64));
    std::string payloadStr(payloadBytes.begin(), payloadBytes.end());

    try {
        json payload = json::parse(payloadStr);

        // Ablauf prüfen
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
        // Entferne Einträge älter als 1 Stunde ohne Aktivität
        return !entry.isLocked &&
               std::chrono::duration_cast<std::chrono::hours>(now - entry.firstAttempt).count() > 1;
    });
}

} // namespace auth
