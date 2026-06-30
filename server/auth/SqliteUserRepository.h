// =============================================================================
// server/auth/SqliteUserRepository.h — SQLite-Implementierung von IUserRepository
// =============================================================================
// KORREKTUR (AP-45 Fix): Produktionsreife SQLite-Implementierung mit
// Argon2id-Hashing. Keine hartkodierten Credentials.
// =============================================================================
#pragma once
#include "IUserRepository.h"
#include <sqlite3.h>
#include <mutex>

namespace auth {

class SqliteUserRepository : public IUserRepository {
    sqlite3* db = nullptr;
    mutable std::mutex dbMutex;

    [[nodiscard]] bool EnsureSchema();

public:
    explicit SqliteUserRepository(const char* dbPath = "auth.db");
    ~SqliteUserRepository() override;

    SqliteUserRepository(const SqliteUserRepository&) = delete;
    SqliteUserRepository& operator=(const SqliteUserRepository&) = delete;

    // IUserRepository Implementation
    [[nodiscard]] std::expected<void, RepositoryError> CreateUser(const UserRecord& user) override;
    [[nodiscard]] std::expected<UserRecord, RepositoryError> FindByUsername(std::string_view username) override;
    [[nodiscard]] std::expected<bool, RepositoryError> UserExists(std::string_view username) override;
    [[nodiscard]] std::expected<void, RepositoryError> UpdatePasswordHash(
        std::string_view username, std::span<const uint8_t> newHash) override;
    [[nodiscard]] std::expected<void, RepositoryError> UpdateLastLogin(
        std::string_view username, std::string_view ip) override;
    [[nodiscard]] std::expected<void, RepositoryError> UpdateLoginAttempts(
        std::string_view username, uint32_t attempts,
        std::chrono::system_clock::time_point lockedUntil) override;
    [[nodiscard]] std::expected<void, RepositoryError> DeleteUser(std::string_view username) override;
    [[nodiscard]] bool IsHealthy() const noexcept override;
};

} // namespace auth
