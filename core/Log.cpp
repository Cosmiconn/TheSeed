// =============================================================================
// core/Log.cpp  —  Log Implementation
// =============================================================================
#include "Log.h"

std::vector<std::string> engineLog;
std::mutex               logMutex;

void AddLog(std::string_view text) {
    std::lock_guard lock(logMutex);
#if ENGINE_BUILD_EDITOR
    engineLog.emplace_back(text);
    if (engineLog.size() > LOG_MAX)
        engineLog.erase(engineLog.begin());
#else
    std::cout << text << '\n';
#endif
}
