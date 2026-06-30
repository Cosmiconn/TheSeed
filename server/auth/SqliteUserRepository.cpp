// =============================================================================
// server/auth/SqliteUserRepository.cpp — SQLite-Implementierung
// =============================================================================
#include "SqliteUserRepository.h"
#include "../../core/Log.h"
#include <cstring>

namespace auth {

// =============================================================================
// Schema Definition
// =============================================================================
static constexpr const char* CREATE_USERS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS users (
        username TEXT PRIMARY KEY NOT NULL,
        password_hash BLOB NOT NULL,
        email TEXT NOT NULL DEFAULT '',
        is_active INTEGER NOT NULL DEFAULT 1,
        is_banned INTEGER NOT NULL DEFAULT 0,
        created_at INTEGER NOT NULL DEFAULT (unixepoch()),
        last_login_at INTEGER,
        last_login_ip TEXT DEFAULT '',
        failed_login_attempts INTEGER NOT NULL DEFAULT 0,
        locked_until INTEGER
    );
    CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
)";

// =============================================================================
// Helper: SQLite Error → RepositoryError
// =============================================================================
static RepositoryError MapSqliteError(int rc) {
    switch (rc) {
        case SQLITE_CONSTRAINT:
            return RepositoryError::DuplicateUser;
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
            return RepositoryError::ConnectionFailed;
        default:
            return RepositoryError::QueryFailed;
    }
}

// =============================================================================
// Helper: Unix Timestamp ↔ chrono
// =============================================================================
static int64_t ToUnixSeconds(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

static std::chrono::system_clock::time_point FromUnixSeconds(int64_t seconds) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
SqliteUserRepository::SqliteUserRepository(const char* dbPath) {
    int rc = sqlite3_open(dbPath, &db);
    if (rc != SQLITE_OK) {
        AddLog("[Auth][SQLite] CRITICAL: Failed to open database: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;
        return;
    }

    // Enable WAL mode for better concurrency
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    if (!EnsureSchema()) {
        AddLog("[Auth][SQLite] CRITICAL: Schema creation failed");
        sqlite3_close(db);
        db = nullptr;
    } else {
        AddLog("[Auth][SQLite] Repository initialized: {}", dbPath);
    }
}

SqliteUserRepository::~SqliteUserRepository() {
    if (db) {
        sqlite3_close(db);
    }
}

bool SqliteUserRepository::EnsureSchema() {
    if (!db) return false;
    return sqlite3_exec(db, CREATE_USERS_TABLE, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SqliteUserRepository::IsHealthy() const noexcept {
    return db != nullptr;
}

// =============================================================================
// CreateUser
// =============================================================================
std::expected<void, RepositoryError> SqliteUserRepository::CreateUser(const UserRecord& user) {
    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = R"(
        INSERT INTO users (username, password_hash, email, is_active, is_banned, created_at)
        VALUES (?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_text(stmt, 1, user.username.data(), static_cast<int>(user.username.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, user.passwordHash.data(), static_cast<int>(user.passwordHash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.email.data(), static_cast<int>(user.email.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, user.isActive ? 1 : 0);
    sqlite3_bind_int(stmt, 5, user.isBanned ? 1 : 0);
    sqlite3_bind_int64(stmt, 6, ToUnixSeconds(user.createdAt));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        AddLog("[Auth][SQLite] User created: {}", user.username);
        return {};
    }

    return std::unexpected(MapSqliteError(rc));
}

// =============================================================================
// FindByUsername
// =============================================================================
std::expected<UserRecord, RepositoryError> SqliteUserRepository::FindByUsername(std::string_view username) {
    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = R"(
        SELECT username, password_hash, email, is_active, is_banned,
               created_at, last_login_at, last_login_ip,
               failed_login_attempts, locked_until
        FROM users WHERE username = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_text(stmt, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);

    UserRecord record;
    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::unexpected(RepositoryError::UserNotFound);
    }

    // Column 0: username
    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (name) record.username = name;

    // Column 1: password_hash (BLOB)
    const void* hashBlob = sqlite3_column_blob(stmt, 1);
    int hashSize = sqlite3_column_bytes(stmt, 1);
    if (hashBlob && hashSize > 0) {
        record.passwordHash.resize(hashSize);
        std::memcpy(record.passwordHash.data(), hashBlob, hashSize);
    }

    // Column 2: email
    const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (email) record.email = email;

    // Column 3: is_active
    record.isActive = sqlite3_column_int(stmt, 3) != 0;

    // Column 4: is_banned
    record.isBanned = sqlite3_column_int(stmt, 4) != 0;

    // Column 5: created_at
    record.createdAt = FromUnixSeconds(sqlite3_column_int64(stmt, 5));

    // Column 6: last_login_at (nullable)
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        record.lastLoginAt = FromUnixSeconds(sqlite3_column_int64(stmt, 6));
    }

    // Column 7: last_login_ip
    const char* ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    if (ip) record.lastLoginIP = ip;

    // Column 8: failed_login_attempts
    record.failedLoginAttempts = static_cast<uint32_t>(sqlite3_column_int(stmt, 8));

    // Column 9: locked_until (nullable)
    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        record.lockedUntil = FromUnixSeconds(sqlite3_column_int64(stmt, 9));
    }

    sqlite3_finalize(stmt);
    return record;
}

// =============================================================================
// UserExists
// =============================================================================
std::expected<bool, RepositoryError> SqliteUserRepository::UserExists(std::string_view username) {
    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = "SELECT 1 FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_text(stmt, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_ROW;
}

// =============================================================================
// UpdatePasswordHash
// =============================================================================
std::expected<void, RepositoryError> SqliteUserRepository::UpdatePasswordHash(
    std::string_view username, std::span<const uint8_t> newHash) {

    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = "UPDATE users SET password_hash = ? WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_blob(stmt, 1, newHash.data(), static_cast<int>(newHash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        AddLog("[Auth][SQLite] Password updated for: {}", username);
        return {};
    }
    return std::unexpected(RepositoryError::QueryFailed);
}

// =============================================================================
// UpdateLastLogin
// =============================================================================
std::expected<void, RepositoryError> SqliteUserRepository::UpdateLastLogin(
    std::string_view username, std::string_view ip) {

    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = R"(
        UPDATE users SET
            last_login_at = ?,
            last_login_ip = ?,
            failed_login_attempts = 0,
            locked_until = NULL
        WHERE username = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_int64(stmt, 1, ToUnixSeconds(std::chrono::system_clock::now()));
    sqlite3_bind_text(stmt, 2, ip.data(), static_cast<int>(ip.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

// =============================================================================
// UpdateLoginAttempts
// =============================================================================
std::expected<void, RepositoryError> SqliteUserRepository::UpdateLoginAttempts(
    std::string_view username, uint32_t attempts,
    std::chrono::system_clock::time_point lockedUntil) {

    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = R"(
        UPDATE users SET
            failed_login_attempts = ?,
            locked_until = ?
        WHERE username = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(attempts));
    sqlite3_bind_int64(stmt, 2, ToUnixSeconds(lockedUntil));
    sqlite3_bind_text(stmt, 3, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) return {};
    return std::unexpected(RepositoryError::QueryFailed);
}

// =============================================================================
// DeleteUser
// =============================================================================
std::expected<void, RepositoryError> SqliteUserRepository::DeleteUser(std::string_view username) {
    if (!db) return std::unexpected(RepositoryError::ConnectionFailed);

    std::lock_guard lock(dbMutex);

    constexpr const char* sql = "DELETE FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::unexpected(RepositoryError::QueryFailed);
    }

    sqlite3_bind_text(stmt, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        AddLog("[Auth][SQLite] User deleted: {}", username);
        return {};
    }
    return std::unexpected(RepositoryError::QueryFailed);
}

} // namespace auth
