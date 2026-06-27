// =============================================================================
// core/Database.cpp  —  Database Implementation
// =============================================================================
#include "Database.h"
#include "GameSystems.h"

std::optional<DatabaseManager> GameDB;

// --- WorkerLoop --------------------------------------------------------------
void DatabaseManager::WorkerLoop(std::stop_token st) {
    while (!st.stop_requested()) {
        std::unique_lock lock(qMutex);
        cv.wait(lock, [this, &st]{ return !writeQueue.empty() || st.stop_requested(); });
        while (!writeQueue.empty()) {
            auto p = writeQueue.front(); writeQueue.pop(); lock.unlock();
            if (db) {
                constexpr const char* q =
                    "INSERT OR REPLACE INTO characters"
                    "(username,level,gold,lastX,lastY,lastZ,lastSector)"
                    " VALUES(?,?,?,?,?,?,?);";
                sqlite3_stmt* s = nullptr;
                if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text  (s, 1, p.username,   -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int   (s, 2, static_cast<int>(p.level));
                    sqlite3_bind_int   (s, 3, static_cast<int>(p.gold));
                    sqlite3_bind_double(s, 4, p.lastX);
                    sqlite3_bind_double(s, 5, p.lastY);
                    sqlite3_bind_double(s, 6, p.lastZ);
                    sqlite3_bind_text  (s, 7, p.lastSector, -1, SQLITE_TRANSIENT);
                    sqlite3_step(s); sqlite3_finalize(s);
                }
            }
            lock.lock();
        }
    }
}

// --- Constructor --------------------------------------------------------------
DatabaseManager::DatabaseManager() {
    if (sqlite3_open("game.db", &db) == SQLITE_OK) {
        constexpr const char* schema1 =
            "CREATE TABLE IF NOT EXISTS characters("
            "username TEXT PRIMARY KEY, level INT, gold INT,"
            "lastX REAL, lastY REAL, lastZ REAL, lastSector TEXT);";
        constexpr const char* schema2 =
            "CREATE TABLE IF NOT EXISTS quest_log("
            "username TEXT, quest_id INT, state INT, progress TEXT,"
            "PRIMARY KEY(username, quest_id));";
        constexpr const char* schema3 =
            "CREATE TABLE IF NOT EXISTS inventory("
            "username TEXT, slot INT, templateId INT, count INT, isEquipped INT,"
            "PRIMARY KEY(username, slot));";
        constexpr const char* schema4 =
            "CREATE TABLE IF NOT EXISTS accounts("
            "username TEXT PRIMARY KEY, password_hash TEXT);";
        sqlite3_exec(db, schema1, nullptr, nullptr, nullptr);
        sqlite3_exec(db, schema2, nullptr, nullptr, nullptr);
        sqlite3_exec(db, schema3, nullptr, nullptr, nullptr);
        sqlite3_exec(db, schema4, nullptr, nullptr, nullptr);
        AddLog("[DB] Schema bereit (4 Tabellen).");
    }
    worker = std::jthread([this](std::stop_token st){ WorkerLoop(st); });
}

// --- Destructor ---------------------------------------------------------------
DatabaseManager::~DatabaseManager() {
    worker.request_stop();
    cv.notify_one();
    if (worker.joinable()) worker.join();
    if (db) sqlite3_close(db);
}

// --- GetProfile ---------------------------------------------------------------
bool DatabaseManager::GetProfile(std::string_view name, PlayerProfile& out) {
    if (!db) return false;
    constexpr const char* q =
        "SELECT level,gold,lastX,lastY,lastZ,lastSector"
        " FROM characters WHERE username=?;";
    sqlite3_stmt* s = nullptr; bool found = false;
    if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
        if (sqlite3_step(s) == SQLITE_ROW) {
            SafeStrCopy(out.username, name, sizeof(out.username));
            out.level  = static_cast<uint32_t>(sqlite3_column_int   (s, 0));
            out.gold   = static_cast<uint32_t>(sqlite3_column_int   (s, 1));
            out.lastX  = static_cast<float>   (sqlite3_column_double(s, 2));
            out.lastY  = static_cast<float>   (sqlite3_column_double(s, 3));
            out.lastZ  = static_cast<float>   (sqlite3_column_double(s, 4));
            const char* sec = reinterpret_cast<const char*>(sqlite3_column_text(s, 5));
            if (sec) SafeStrCopy(out.lastSector, std::string_view(sec), sizeof(out.lastSector));
            found = true;
        }
        sqlite3_finalize(s);
    }
    return found;
}

// --- LoadInventory ------------------------------------------------------------
void DatabaseManager::LoadInventory(std::string_view username, std::vector<Item>& inv) {
    inv.assign(INVENTORY_SIZE, Item{}); if (!db) return;
    constexpr const char* q =
        "SELECT slot,templateId,count,isEquipped"
        " FROM inventory WHERE username=?;";
    sqlite3_stmt* s = nullptr;
    if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
        while (sqlite3_step(s) == SQLITE_ROW) {
            size_t slot = static_cast<size_t>(sqlite3_column_int(s, 0));
            if (slot < inv.size()) {
                inv[slot].templateId = static_cast<uint32_t>(sqlite3_column_int(s, 1));
                inv[slot].count      = static_cast<uint32_t>(sqlite3_column_int(s, 2));
                inv[slot].isEquipped = sqlite3_column_int(s, 3) != 0;
            }
        }
        sqlite3_finalize(s);
    }
}

// --- Push ----------------------------------------------------------------------
void DatabaseManager::Push(const PlayerProfile& p) {
    std::lock_guard lock(qMutex);
    writeQueue.push(p); cv.notify_one();
}

// --- SaveQuestLog -------------------------------------------------------------
void DatabaseManager::SaveQuestLog(std::string_view username,
                                   const std::vector<PlayerQuestProgress>& log) {
    if (!db) return;
    for (const auto& progress : log) {
        std::string prog;
        for (const auto& obj : progress.objectives)
            prog += std::format("{};", obj.currentCount);
        constexpr const char* q =
            "INSERT OR REPLACE INTO quest_log"
            "(username,quest_id,state,progress) VALUES(?,?,?,?);";
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
            sqlite3_bind_int (s, 2, static_cast<int>(progress.questId));
            sqlite3_bind_int (s, 3, static_cast<int>(progress.state));
            sqlite3_bind_text(s, 4, prog.c_str(),    -1, SQLITE_TRANSIENT);
            sqlite3_step(s); sqlite3_finalize(s);
        }
    }
}

// --- SaveInventory ------------------------------------------------------------
void DatabaseManager::SaveInventory(std::string_view username, const std::vector<Item>& inv) {
    if (!db) return;
    for (size_t slot = 0; slot < inv.size(); ++slot) {
        if (inv[slot].templateId == 0) {
            constexpr const char* q =
                "DELETE FROM inventory WHERE username=? AND slot=?;";
            sqlite3_stmt* s = nullptr;
            if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
                sqlite3_bind_int (s, 2, static_cast<int>(slot));
                sqlite3_step(s); sqlite3_finalize(s);
            }
        } else {
            constexpr const char* q =
                "INSERT OR REPLACE INTO inventory"
                "(username,slot,templateId,count,isEquipped)"
                " VALUES(?,?,?,?,?);";
            sqlite3_stmt* s = nullptr;
            if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
                sqlite3_bind_int (s, 2, static_cast<int>(slot));
                sqlite3_bind_int (s, 3, static_cast<int>(inv[slot].templateId));
                sqlite3_bind_int (s, 4, static_cast<int>(inv[slot].count));
                sqlite3_bind_int (s, 5, inv[slot].isEquipped ? 1 : 0);
                sqlite3_step(s); sqlite3_finalize(s);
            }
        }
    }
}

// =============================================================================
// Account Functions
// =============================================================================
bool RegisterAccount(std::string_view username, std::string_view password) {
    if (!GameDB || !GameDB->GetDB()) return false;
    std::string hash = Argon2IdHash(password, "MMORPGEngineV12Salt");
    constexpr const char* q = "INSERT OR IGNORE INTO accounts(username,password_hash) VALUES(?,?);";
    sqlite3_stmt* s = nullptr;
    if (sqlite3_prepare_v2(GameDB->GetDB(), q, -1, &s, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 2, hash.c_str(),     -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(s); sqlite3_finalize(s);
        return rc == SQLITE_DONE;
    }
    return false;
}

bool VerifyAccount(std::string_view username, std::string_view password) {
    if (!GameDB || !GameDB->GetDB()) return false;
    std::string expected = Argon2IdHash(password, "MMORPGEngineV12Salt");
    constexpr const char* q = "SELECT password_hash FROM accounts WHERE username=?;";
    sqlite3_stmt* s = nullptr; bool valid = false;
    if (sqlite3_prepare_v2(GameDB->GetDB(), q, -1, &s, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);
        if (sqlite3_step(s) == SQLITE_ROW) {
            const char* h = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
            if (h && std::string_view(h) == expected) valid = true;
        }
        sqlite3_finalize(s);
    }
    return valid;
}
