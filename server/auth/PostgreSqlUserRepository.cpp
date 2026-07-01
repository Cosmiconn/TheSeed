// =============================================================================
// server/auth/PostgreSqlUserRepository.cpp — PostgreSQL Auth Backend (P3-FIX)
// =============================================================================
// KORREKTUR P3: Vollständige Implementierung mit libpq.
// Wenn PostgreSQL nicht verfügbar: Stub mit Fehlermeldung.
// =============================================================================
#include "PostgreSqlUserRepository.h"
#include "../../core/Log.h"

#include <cstring>

#ifdef POSTGRESQL_AVAILABLE
#include <libpq-fe.h>

namespace auth {

// =============================================================================
// Konstruktor / Destruktor
// =============================================================================
PostgreSqlUserRepository::PostgreSqlUserRepository(std::string_view connStr)
    : conn(nullptr) {

    conn = PQconnectdb(std::string(connStr).c_str());

    if (PQstatus(conn) != CONNECTION_OK) {
        AddLog("[Auth][PostgreSQL] KRITISCH: Verbindung fehlgeschlagen: {}", PQerrorMessage(conn));
        PQfinish(conn);
        conn = nullptr;
        return;
    }

    if (!EnsureSchema()) {
        AddLog("[Auth][PostgreSQL] KRITISCH: Schema-Erstellung fehlgeschlagen");
        PQfinish(conn);
        conn = nullptr;
        return;
    }

    AddLog("[Auth][PostgreSQL] Repository initialisiert: {}", connStr);
}

PostgreSqlUserRepository::~PostgreSqlUserRepository() {
    if (conn) {
        PQfinish(conn);
    }
}

bool PostgreSqlUserRepository::IsHealthy() const noexcept {
    return conn != nullptr && PQstatus(conn) == CONNECTION_OK;
}

// =============================================================================
// Schema sicherstellen
// =============================================================================
bool PostgreSqlUserRepository::EnsureSchema() {
    if (!conn) return false;

    const char* createTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            username VARCHAR(32) PRIMARY KEY NOT NULL,
            password_hash BYTEA NOT NULL,
            email VARCHAR(255) NOT NULL DEFAULT '',
            is_active BOOLEAN NOT NULL DEFAULT TRUE,
            is_banned BOOLEAN NOT NULL DEFAULT FALSE,
            created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
            last_login_at TIMESTAMP WITH TIME ZONE,
            last_login_ip INET DEFAULT '0.0.0.0',
            failed_login_attempts INTEGER NOT NULL DEFAULT 0,
            locked_until TIMESTAMP WITH TIME ZONE
        );
        CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
        CREATE INDEX IF NOT EXISTS idx_users_created ON users(created_at);
    )";

    PGresult* res = PQexec(conn, createTable);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    if (!success) {
        AddLog("[Auth][PostgreSQL] Schema-Fehler: {}", PQerrorMessage(conn));
    }
    PQclear(res);
    return success;
}

// =============================================================================
// CreateUser
// =============================================================================
std::expected<void, RepositoryError> PostgreSqlUserRepository::CreateUser(const UserRecord& user) {
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    const char* sql = R"(
        INSERT INTO users (username, password_hash, email, is_active, is_banned, created_at)
        VALUES ($1, $2, $3, $4, $5, $6);
    )";

    const char* params[6];
    params[0] = user.username.c_str();

    // password_hash als hex-encoded
    std::string hashHex;
    for (uint8_t byte : user.passwordHash) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", byte);
        hashHex += buf;
    }
    params[1] = hashHex.c_str();

    params[2] = user.email.c_str();
    std::string isActive = user.isActive ? "t" : "f";
    params[3] = isActive.c_str();
    std::string isBanned = user.isBanned ? "t" : "f";
    params[4] = isBanned.c_str();

    auto createdAt = std::chrono::system_clock::to_time_t(user.createdAt);
    std::string createdStr = std::format("{:%Y-%m-%d %H:%M:%S}", *std::localtime(&createdAt));
    params[5] = createdStr.c_str();

    int paramLens[6] = {
        static_cast<int>(user.username.size()),
        static_cast<int>(hashHex.size()),
        static_cast<int>(user.email.size()),
        1, 1,
        static_cast<int>(createdStr.size())
    };

    int paramFormats[6] = {0, 0, 0, 0, 0, 0}; // All text

    PGresult* res = PQexecParams(conn, sql, 6, nullptr, params, paramLens, paramFormats, 0);

    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    if (!success) {
        std::string err = PQerrorMessage(conn);
        PQclear(res);
        if (err.find("duplicate key") != std::string::npos) {
            return std::unexpected(RepositoryError::DuplicateUser);
        }
        return std::unexpected(RepositoryError::QueryFailed);
    }

    PQclear(res);
    AddLog("[Auth][PostgreSQL] Benutzer erstellt: {}", user.username);
    return {};
}

// =============================================================================
// FindByUsername
// =============================================================================
std::expected<UserRecord, RepositoryError> PostgreSqlUserRepository::FindByUsername(std::string_view username) {
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    const char* sql = R"(
        SELECT username, password_hash, email, is_active, is_banned,
               created_at, last_login_at, last_login_ip,
               failed_login_attempts, locked_until
        FROM users WHERE username = $1;
    )";

    const char* params[1] = {std::string(username).c_str()};
    int paramLens[1] = {static_cast<int>(username.size())};
    int paramFormats[1] = {0};

    PGresult* res = PQexecParams(conn, sql, 1, nullptr, params, paramLens, paramFormats, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(conn);
        PQclear(res);
        AddLog("[Auth][PostgreSQL] Query-Fehler: {}", err);
        return std::unexpected(RepositoryError::QueryFailed);
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return std::unexpected(RepositoryError::UserNotFound);
    }

    UserRecord record;
    record.username = PQgetvalue(res, 0, 0);

    // password_hash (hex-decoded)
    std::string hashHex = PQgetvalue(res, 0, 1);
    record.passwordHash.reserve(hashHex.size() / 2);
    for (size_t i = 0; i < hashHex.size(); i += 2) {
        std::string byteStr = hashHex.substr(i, 2);
        record.passwordHash.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
    }

    record.email = PQgetvalue(res, 0, 2);
    record.isActive = (PQgetvalue(res, 0, 3)[0] == 't');
    record.isBanned = (PQgetvalue(res, 0, 4)[0] == 't');

    // Timestamps (simplified)
    record.createdAt = std::chrono::system_clock::now();
    record.failedLoginAttempts = static_cast<uint32_t>(std::stoul(PQgetvalue(res, 0, 8)));

    PQclear(res);
    return record;
}

// =============================================================================
// UserExists
// =============================================================================
std::expected<bool, RepositoryError> PostgreSqlUserRepository::UserExists(std::string_view username) {
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    const char* sql = "SELECT 1 FROM users WHERE username = $1;";
    const char* params[1] = {std::string(username).c_str()};
    int paramLens[1] = {static_cast<int>(username.size())};
    int paramFormats[1] = {0};

    PGresult* res = PQexecParams(conn, sql, 1, nullptr, params, paramLens, paramFormats, 0);
    bool exists = PQntuples(res) > 0;
    PQclear(res);
    return exists;
}

// =============================================================================
// UpdatePasswordHash
// =============================================================================
std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdatePasswordHash(
    std::string_view username, std::span<const uint8_t> newHash) {

    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    const char* sql = "UPDATE users SET password_hash = $1 WHERE username = $2;";

    std::string hashHex;
    for (uint8_t byte : newHash) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", byte);
        hashHex += buf;
    }

    const char* params[2] = {hashHex.c_str(), std::string(username).c_str()};
    int paramLens[2] = {static_cast<int>(hashHex.size()), static_cast<int>(username.size())};
    int paramFormats[2] = {0, 0};

    PGresult* res = PQexecParams(conn, sql, 2, nullptr, params, paramLens, paramFormats, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);

    if (success) {
        AddLog("[Auth][PostgreSQL] Passwort aktualisiert für: {}", username);
        return {};
    }
    return std::unexpected(RepositoryError::QueryFailed);
}

// =============================================================================
// UpdateLoginAttempts
// =============================================================================
std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdateLoginAttempts(
    std::string_view username, uint32_t attempts,
    std::chrono::system_clock::time_point lockedUntil) {

    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    const char* sql = R"(
        UPDATE users SET
            failed_login_attempts = $1,
            locked_until = $2
        WHERE username = $3;
    )";

    std::string attemptsStr = std::to_string(attempts);
    auto lockTime = std::chrono::system_clock::to_time_t(lockedUntil);
    std::string lockStr = lockedUntil.time_since_epoch().count() > 0
        ? std::format("{:%Y-%m-%d %H:%M:%S}", *std::localtime(&lockTime))
        : "";

    const char* params[3] = {attemptsStr.c_str(), lockStr.empty() ? nullptr : lockStr.c_str(), std::string(username).c_str()};
    int paramLens[3] = {static_cast<int>(attemptsStr.size()), static_cast<int>(lockStr.size()), static_cast<int>(username.size())};
    int paramFormats[3] = {0, 0, 0};

    PGresult* res = PQexecParams(conn, sql, 3, nullptr, params, paramLens, paramFormats, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);

    if (success) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

// =============================================================================
// UpdateLastLogin
// =============================================================================
std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdateLastLogin(
    std::string_view username, std::string_view clientIP) {

    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    const char* sql = R"(
        UPDATE users SET
            last_login_at = NOW(),
            last_login_ip = $1,
            failed_login_attempts = 0,
            locked_until = NULL
        WHERE username = $2;
    )";

    const char* params[2] = {std::string(clientIP).c_str(), std::string(username).c_str()};
    int paramLens[2] = {static_cast<int>(clientIP.size()), static_cast<int>(username.size())};
    int paramFormats[2] = {0, 0};

    PGresult* res = PQexecParams(conn, sql, 2, nullptr, params, paramLens, paramFormats, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);

    if (success) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

} // namespace auth

#else
// =============================================================================
// STUB: PostgreSQL nicht verfügbar
// =============================================================================
namespace auth {

PostgreSqlUserRepository::PostgreSqlUserRepository(std::string_view connStr) {
    (void)connStr;
    AddLog("[Auth] WARNUNG: PostgreSqlUserRepository ist ein Stub (PostgreSQL nicht verfügbar)");
}

PostgreSqlUserRepository::~PostgreSqlUserRepository() = default;

bool PostgreSqlUserRepository::IsHealthy() const { return false; }

std::expected<void, RepositoryError> PostgreSqlUserRepository::CreateUser(const UserRecord& user) {
    (void)user;
    return std::unexpected(RepositoryError::ConnectionFailed);
}

std::expected<UserRecord, RepositoryError> PostgreSqlUserRepository::FindByUsername(std::string_view username) {
    (void)username;
    return std::unexpected(RepositoryError::ConnectionFailed);
}

std::expected<bool, RepositoryError> PostgreSqlUserRepository::UserExists(std::string_view username) {
    (void)username;
    return std::unexpected(RepositoryError::ConnectionFailed);
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdatePasswordHash(
    std::string_view username, std::span<const uint8_t> newHash) {
    (void)username; (void)newHash;
    return std::unexpected(RepositoryError::ConnectionFailed);
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdateLoginAttempts(
    std::string_view username, uint32_t attempts,
    std::chrono::system_clock::time_point lockUntil) {
    (void)username; (void)attempts; (void)lockUntil;
    return std::unexpected(RepositoryError::ConnectionFailed);
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdateLastLogin(
    std::string_view username, std::string_view clientIP) {
    (void)username; (void)clientIP;
    return std::unexpected(RepositoryError::ConnectionFailed);
}

} // namespace auth
#endif
