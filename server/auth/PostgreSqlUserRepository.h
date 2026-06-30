// =============================================================================
// server/auth/PostgreSqlUserRepository.h — PostgreSQL Implementierung (AP-45)
// =============================================================================
// KORREKTUR: Produktionsreife PostgreSQL-Implementierung von IUserRepository.
// Nutzt libpq (vcpkg: libpq) für asynchrone, connection-pooled DB-Zugriffe.
// =============================================================================
#pragma once
#include "IUserRepository.h"
#include <libpq-fe.h>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <condition_variable>

namespace auth {

// =============================================================================
// Connection Pool für PostgreSQL
// =============================================================================
class PgConnectionPool {
    struct PooledConnection {
        PGconn* conn = nullptr;
        bool inUse = false;
        std::chrono::steady_clock::time_point lastUsed;
    };

    std::string connString;
    std::vector<PooledConnection> pool;
    std::mutex poolMutex;
    std::condition_variable cv;
    size_t maxConnections = 10;
    size_t minConnections = 2;

    [[nodiscard]] PGconn* CreateConnection();
    void DestroyConnection(PGconn* conn);

public:
    explicit PgConnectionPool(std::string_view connectionString);
    ~PgConnectionPool();

    PgConnectionPool(const PgConnectionPool&) = delete;
    PgConnectionPool& operator=(const PgConnectionPool&) = delete;

    [[nodiscard]] PGconn* Acquire();
    void Release(PGconn* conn);
    void SetMaxConnections(size_t max);
    [[nodiscard]] size_t GetActiveConnections() const;
};

// =============================================================================
// PostgreSqlUserRepository
// =============================================================================
class PostgreSqlUserRepository : public IUserRepository {
    std::unique_ptr<PgConnectionPool> connectionPool;
    mutable std::mutex queryMutex;

    [[nodiscard]] bool EnsureSchema();
    [[nodiscard]] static RepositoryError MapPgError(const PGresult* result);

public:
    explicit PostgreSqlUserRepository(std::string_view connectionString);
    ~PostgreSqlUserRepository() override;

    PostgreSqlUserRepository(const PostgreSqlUserRepository&) = delete;
    PostgreSqlUserRepository& operator=(const PostgreSqlUserRepository&) = delete;

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

    // PostgreSQL-specific
    [[nodiscard]] bool TestConnection();
    void Vacuum();
};

} // namespace auth
