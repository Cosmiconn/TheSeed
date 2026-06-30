// =============================================================================
// server/auth/IUserRepository.h — Abstraktes User-Repository Interface
// =============================================================================
// KORREKTUR (AP-45 Fix): Entkopplung von AuthService und konkreter DB.
// Ermöglicht SQLite (Dev), PostgreSQL (Prod) und Mock (Tests) ohne Änderung
// am AuthService.
// =============================================================================
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <expected>
#include <chrono>

namespace auth {

enum class RepositoryError {
    ConnectionFailed,
    QueryFailed,
    UserNotFound,
    DuplicateUser,
    InternalError
};

// =============================================================================
// UserRecord — Domain-Modell (unabhängig von DB-Schema)
// =============================================================================
struct UserRecord {
    std::string username;
    std::vector<uint8_t> passwordHash;  // Argon2id Hash (libsodium format)
    std::string email;
    bool isActive = true;
    bool isBanned = false;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastLoginAt;
    std::string lastLoginIP;
    uint32_t failedLoginAttempts = 0;
    std::chrono::system_clock::time_point lockedUntil;
};

// =============================================================================
// IUserRepository — Pure Interface (C++23: keine virtual destructor nötig,
// aber empfohlen für polymorphen Delete)
// =============================================================================
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    // Create
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        CreateUser(const UserRecord& user) = 0;

    // Read
    [[nodiscard]] virtual std::expected<UserRecord, RepositoryError>
        FindByUsername(std::string_view username) = 0;

    [[nodiscard]] virtual std::expected<bool, RepositoryError>
        UserExists(std::string_view username) = 0;

    // Update
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        UpdatePasswordHash(std::string_view username, std::span<const uint8_t> newHash) = 0;

    [[nodiscard]] virtual std::expected<void, RepositoryError>
        UpdateLastLogin(std::string_view username, std::string_view ip) = 0;

    [[nodiscard]] virtual std::expected<void, RepositoryError>
        UpdateLoginAttempts(std::string_view username, uint32_t attempts,
            std::chrono::system_clock::time_point lockedUntil) = 0;

    // Delete
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        DeleteUser(std::string_view username) = 0;

    // Health check
    [[nodiscard]] virtual bool IsHealthy() const noexcept = 0;
};

} // namespace auth
