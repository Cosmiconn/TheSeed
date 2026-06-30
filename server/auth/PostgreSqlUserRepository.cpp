// =============================================================================
// server/auth/PostgreSqlUserRepository.cpp — PostgreSQL Implementierung
// =============================================================================
#include "PostgreSqlUserRepository.h"
#include "../../core/Log.h"
#include <cstring>
#include <chrono>

namespace auth {

// =============================================================================
// Connection Pool Implementation
// =============================================================================
PgConnectionPool::PgConnectionPool(std::string_view connectionString)
    : connString(connectionString) {
    // Create minimum connections
    for (size_t i = 0; i < minConnections; ++i) {
        PGconn* conn = CreateConnection();
        if (conn) {
            pool.push_back(PooledConnection{conn, false, std::chrono::steady_clock::now()});
        }
    }
    AddLog("[Auth][PostgreSQL] Connection pool initialized: {}/{} connections",
           pool.size(), maxConnections);
}

PgConnectionPool::~PgConnectionPool() {
    for (auto& pc : pool) {
        if (pc.conn) {
            PQfinish(pc.conn);
        }
    }
}

PGconn* PgConnectionPool::CreateConnection() {
    PGconn* conn = PQconnectdb(connString.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        AddLog("[Auth][PostgreSQL] Connection failed: {}", PQerrorMessage(conn));
        PQfinish(conn);
        return nullptr;
    }
    return conn;
}

void PgConnectionPool::DestroyConnection(PGconn* conn) {
    if (conn) PQfinish(conn);
}

PGconn* PgConnectionPool::Acquire() {
    std::unique_lock lock(poolMutex);

    // Wait for available connection
    cv.wait(lock, [this] {
        for (auto& pc : pool) {
            if (!pc.inUse) return true;
        }
        return pool.size() < maxConnections;
    });

    // Find available connection
    for (auto& pc : pool) {
        if (!pc.inUse) {
            pc.inUse = true;
            pc.lastUsed = std::chrono::steady_clock::now();
            return pc.conn;
        }
    }

    // Create new connection if under limit
    if (pool.size() < maxConnections) {
        PGconn* conn = CreateConnection();
        if (conn) {
            pool.push_back(PooledConnection{conn, true, std::chrono::steady_clock::now()});
            return conn;
        }
    }

    return nullptr;
}

void PgConnectionPool::Release(PGconn* conn) {
    std::lock_guard lock(poolMutex);
    for (auto& pc : pool) {
        if (pc.conn == conn) {
            pc.inUse = false;
            pc.lastUsed = std::chrono::steady_clock::now();
            break;
        }
    }
    cv.notify_one();
}

void PgConnectionPool::SetMaxConnections(size_t max) {
    maxConnections = max;
}

size_t PgConnectionPool::GetActiveConnections() const {
    std::lock_guard lock(poolMutex);
    size_t count = 0;
    for (const auto& pc : pool) {
        if (pc.inUse) count++;
    }
    return count;
}

// =============================================================================
// PostgreSqlUserRepository Implementation
// =============================================================================
PostgreSqlUserRepository::PostgreSqlUserRepository(std::string_view connectionString) {
    connectionPool = std::make_unique<PgConnectionPool>(connectionString);
    if (IsHealthy()) {
        EnsureSchema();
    }
}

PostgreSqlUserRepository::~PostgreSqlUserRepository() = default;

bool PostgreSqlUserRepository::IsHealthy() const noexcept {
    return connectionPool && connectionPool->GetActiveConnections() >= 0;
}

bool PostgreSqlUserRepository::TestConnection() {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return false;

    PGresult* res = PQexec(conn, "SELECT 1");
    bool ok = PQresultStatus(res) == PGRES_TUPLES_OK;
    PQclear(res);
    connectionPool->Release(conn);
    return ok;
}

RepositoryError PostgreSqlUserRepository::MapPgError(const PGresult* result) {
    if (!result) return RepositoryError::QueryFailed;
    ExecStatusType status = PQresultStatus(result);
    switch (status) {
        case PGRES_FATAL_ERROR:
            return RepositoryError::ConnectionFailed;
        case PGRES_NONFATAL_ERROR:
            return RepositoryError::QueryFailed;
        default:
            return RepositoryError::InternalError;
    }
}

bool PostgreSqlUserRepository::EnsureSchema() {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return false;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            username VARCHAR(32) PRIMARY KEY NOT NULL,
            password_hash BYTEA NOT NULL,
            email VARCHAR(255) NOT NULL DEFAULT '',
            is_active BOOLEAN NOT NULL DEFAULT TRUE,
            is_banned BOOLEAN NOT NULL DEFAULT FALSE,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
            last_login_at TIMESTAMP WITH TIME ZONE,
            last_login_ip INET DEFAULT '0.0.0.0',
            failed_login_attempts INTEGER NOT NULL DEFAULT 0,
            locked_until TIMESTAMP WITH TIME ZONE
        );
        CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
        CREATE INDEX IF NOT EXISTS idx_users_created ON users(created_at);
    )";

    PGresult* res = PQexec(conn, schema);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    if (!success) {
        AddLog("[Auth][PostgreSQL] Schema creation failed: {}", PQerrorMessage(conn));
    }
    PQclear(res);
    connectionPool->Release(conn);
    return success;
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::CreateUser(const UserRecord& user) {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = "INSERT INTO users (username, password_hash, email, created_at) VALUES ($1, $2, $3, NOW())";

    const char* params[3];
    int paramLengths[3];
    int paramFormats[3] = {0, 1, 0}; // text, binary, text

    params[0] = user.username.c_str();
    paramLengths[0] = static_cast<int>(user.username.length());

    params[1] = reinterpret_cast<const char*>(user.passwordHash.data());
    paramLengths[1] = static_cast<int>(user.passwordHash.size());

    params[2] = user.email.c_str();
    paramLengths[2] = static_cast<int>(user.email.length());

    PGresult* res = PQexecParams(conn, sql, 3, nullptr, params, paramLengths, paramFormats, 0);

    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    connectionPool->Release(conn);

    if (success) {
        AddLog("[Auth][PostgreSQL] User created: {}", user.username);
        return {};
    }
    return std::unexpected(RepositoryError::QueryFailed);
}

std::expected<UserRecord, RepositoryError> PostgreSqlUserRepository::FindByUsername(std::string_view username) {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = R"(
        SELECT username, password_hash, email, is_active, is_banned,
               created_at, last_login_at, last_login_ip,
               failed_login_attempts, locked_until
        FROM users WHERE username = $1
    )";

    const char* params[1] = {username.data()};
    int paramLengths[1] = {static_cast<int>(username.length())};

    PGresult* res = PQexecParams(conn, sql, 1, nullptr, params, paramLengths, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        connectionPool->Release(conn);
        return std::unexpected(RepositoryError::UserNotFound);
    }

    UserRecord record;
    record.username = PQgetvalue(res, 0, 0);

    int hashLen = PQgetlength(res, 0, 1);
    const char* hashData = PQgetvalue(res, 0, 1);
    record.passwordHash.resize(hashLen);
    std::memcpy(record.passwordHash.data(), hashData, hashLen);

    record.email = PQgetvalue(res, 0, 2);
    record.isActive = std::strcmp(PQgetvalue(res, 0, 3), "t") == 0;
    record.isBanned = std::strcmp(PQgetvalue(res, 0, 4), "t") == 0;

    // Parse timestamps (simplified)
    record.createdAt = std::chrono::system_clock::now();
    record.failedLoginAttempts = static_cast<uint32_t>(std::atoi(PQgetvalue(res, 0, 8)));

    PQclear(res);
    connectionPool->Release(conn);
    return record;
}

std::expected<bool, RepositoryError> PostgreSqlUserRepository::UserExists(std::string_view username) {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = "SELECT 1 FROM users WHERE username = $1";
    const char* params[1] = {username.data()};
    int paramLengths[1] = {static_cast<int>(username.length())};

    PGresult* res = PQexecParams(conn, sql, 1, nullptr, params, paramLengths, nullptr, 0);
    bool exists = PQntuples(res) > 0;
    PQclear(res);
    connectionPool->Release(conn);
    return exists;
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdatePasswordHash(
    std::string_view username, std::span<const uint8_t> newHash) {

    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = "UPDATE users SET password_hash = $2 WHERE username = $1";
    const char* params[2];
    int paramLengths[2];
    int paramFormats[2] = {0, 1};

    params[0] = username.data();
    paramLengths[0] = static_cast<int>(username.length());
    params[1] = reinterpret_cast<const char*>(newHash.data());
    paramLengths[1] = static_cast<int>(newHash.size());

    PGresult* res = PQexecParams(conn, sql, 2, nullptr, params, paramLengths, paramFormats, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    connectionPool->Release(conn);

    if (success) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdateLastLogin(
    std::string_view username, std::string_view ip) {

    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = R"(
        UPDATE users SET
            last_login_at = NOW(),
            last_login_ip = $2,
            failed_login_attempts = 0,
            locked_until = NULL
        WHERE username = $1
    )";

    const char* params[2] = {username.data(), ip.data()};
    int paramLengths[2] = {static_cast<int>(username.length()), static_cast<int>(ip.length())};

    PGresult* res = PQexecParams(conn, sql, 2, nullptr, params, paramLengths, nullptr, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    connectionPool->Release(conn);

    if (success) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::UpdateLoginAttempts(
    std::string_view username, uint32_t attempts,
    std::chrono::system_clock::time_point lockedUntil) {

    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = R"(
        UPDATE users SET
            failed_login_attempts = $2,
            locked_until = $3
        WHERE username = $1
    )";

    auto lockedTime = std::chrono::system_clock::to_time_t(lockedUntil);
    std::string lockedStr = std::format("{:%Y-%m-%d %H:%M:%S}", *std::gmtime(&lockedTime));

    std::string attemptsStr = std::to_string(attempts);
    const char* params[3] = {username.data(), attemptsStr.c_str(), lockedStr.c_str()};
    int paramLengths[3] = {static_cast<int>(username.length()), static_cast<int>(attemptsStr.length()), static_cast<int>(lockedStr.length())};

    PGresult* res = PQexecParams(conn, sql, 3, nullptr, params, paramLengths, nullptr, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    connectionPool->Release(conn);

    if (success) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

std::expected<void, RepositoryError> PostgreSqlUserRepository::DeleteUser(std::string_view username) {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return std::unexpected(RepositoryError::ConnectionFailed);

    const char* sql = "DELETE FROM users WHERE username = $1";
    const char* params[1] = {username.data()};
    int paramLengths[1] = {static_cast<int>(username.length())};

    PGresult* res = PQexecParams(conn, sql, 1, nullptr, params, paramLengths, nullptr, 0);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    connectionPool->Release(conn);

    if (success) {
        AddLog("[Auth][PostgreSQL] User deleted: {}", username);
        return {};
    }
    return std::unexpected(RepositoryError::QueryFailed);
}

void PostgreSqlUserRepository::Vacuum() {
    PGconn* conn = connectionPool->Acquire();
    if (!conn) return;

    PGresult* res = PQexec(conn, "VACUUM ANALYZE users");
    PQclear(res);
    connectionPool->Release(conn);
}

} // namespace auth
