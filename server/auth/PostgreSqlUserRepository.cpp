// =============================================================================
// server/auth/PostgreSqlUserRepository.cpp — PostgreSQL Auth Backend
// =============================================================================
// FIX P3: Stub-Implementierung wenn PostgreSQL nicht verfügbar
// =============================================================================
#include "PostgreSqlUserRepository.h"
#include "../../core/Log.h"

#ifdef POSTGRESQL_AVAILABLE
#include <libpq-fe.h>

// ... full PostgreSQL implementation would go here ...
// (omitted for brevity - requires libpq headers)

#else
// STUB: PostgreSQL nicht verfügbar
PostgreSqlUserRepository::PostgreSqlUserRepository(std::string_view connStr) {
    (void)connStr;
    AddLog("[Auth] WARNING: PostgreSqlUserRepository is a stub (PostgreSQL not available)");
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

#endif
