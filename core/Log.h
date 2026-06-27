#pragma once
// =============================================================================
// core/Log.h  —  C++23 Modernized
// extern-Deklarationen, Implementation in Log.cpp
// =============================================================================
#include "Types.h"
#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <format>
#include <string_view>

extern std::vector<std::string> engineLog;
extern std::mutex               logMutex;
inline constexpr size_t         LOG_MAX = 200;

void AddLog(std::string_view text);

template<typename... Args>
void AddLog(std::format_string<Args...> fmt, Args&&... args) {
    AddLog(std::format(fmt, std::forward<Args>(args)...));
}
