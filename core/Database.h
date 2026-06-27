#pragma once
// =============================================================================
// core/Database.h  —  C++23 Modernized
// Klassen-Deklaration only. Implementation in Database.cpp
// =============================================================================
#include "ECS.h"
#include "World.h"
#include "Log.h"
#include <sqlite3.h>
#include <thread>
#include <mutex>
#include <queue>
#include <stop_token>
#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <optional>
#include <expected>
#include <format>
#include <string_view>

class DatabaseManager {
    sqlite3*                  db = nullptr;
    std::jthread              worker;
    std::mutex                qMutex;
    std::queue<PlayerProfile> writeQueue;
    std::condition_variable   cv;

    void WorkerLoop(std::stop_token st);

public:
    [[nodiscard]] sqlite3* GetDB() const noexcept { return db; }

    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    [[nodiscard]] bool GetProfile(std::string_view name, PlayerProfile& out);
    void LoadInventory(std::string_view username, std::vector<Item>& inv);
    void Push(const PlayerProfile& p);
    void SaveQuestLog(std::string_view username,
                      const std::vector<PlayerQuestProgress>& log);
    void SaveInventory(std::string_view username, const std::vector<Item>& inv);
};

extern std::optional<DatabaseManager> GameDB;

[[nodiscard]] bool RegisterAccount(std::string_view username, std::string_view password);
[[nodiscard]] bool VerifyAccount(std::string_view username, std::string_view password);
